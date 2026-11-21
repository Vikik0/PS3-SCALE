#include "json_min.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
typedef struct { const char *json; int pos, length, count, max_tokens; cb_json_token *tokens; int failed; } parser;
static void skip_ws(parser *p) { while (p->pos < p->length && isspace((unsigned char)p->json[p->pos])) p->pos++; }
static int add_token(parser *p, cb_json_type type, int start, int parent) {
    int id; if (p->count >= p->max_tokens) { p->failed = 1; return -1; }
    id = p->count++; p->tokens[id].type = type; p->tokens[id].start = start; p->tokens[id].end = start; p->tokens[id].parent = parent; return id;
}
static int parse_string(parser *p, int parent) {
    int id, start; if (p->pos >= p->length || p->json[p->pos] != '"') { p->failed = 1; return -1; }
    start = p->pos; id = add_token(p, CB_JSON_STRING, start, parent); if (id < 0) return -1; p->pos++;
    while (p->pos < p->length) {
        unsigned char c = (unsigned char)p->json[p->pos++];
        if (c == '"') { p->tokens[id].end = p->pos; return id; }
        if (c < 0x20) { p->failed = 1; return -1; }
        if (c == '\\') {
            int i; if (p->pos >= p->length) { p->failed = 1; return -1; }
            c = (unsigned char)p->json[p->pos++];
            if (c == 'u') { for (i = 0; i < 4; i++) if (p->pos >= p->length || !isxdigit((unsigned char)p->json[p->pos++])) { p->failed = 1; return -1; } }
            else if (c != '"' && c != '\\' && c != '/' && c != 'b' && c != 'f' && c != 'n' && c != 'r' && c != 't') { p->failed = 1; return -1; }
        }
    }
    p->failed = 1; return -1;
}
static int parse_value(parser *p, int parent, int depth) {
    int id, start; char c;
    if (depth > 32) { p->failed = 1; return -1; }
    skip_ws(p); if (p->pos >= p->length) { p->failed = 1; return -1; }
    c = p->json[p->pos]; if (c == '"') return parse_string(p, parent);
    if (c == '{' || c == '[') {
        cb_json_type type = c == '{' ? CB_JSON_OBJECT : CB_JSON_ARRAY; char close = c == '{' ? '}' : ']';
        start = p->pos; id = add_token(p, type, start, parent); if (id < 0) return -1; p->pos++; skip_ws(p);
        if (p->pos < p->length && p->json[p->pos] == close) { p->pos++; p->tokens[id].end = p->pos; return id; }
        while (p->pos < p->length && !p->failed) {
            if (type == CB_JSON_OBJECT) {
                if (parse_string(p, id) < 0) return -1;
                skip_ws(p);
                if (p->pos >= p->length || p->json[p->pos++] != ':') { p->failed = 1; return -1; }
                if (parse_value(p, id, depth + 1) < 0) return -1;
            } else if (parse_value(p, id, depth + 1) < 0) return -1;
            skip_ws(p); if (p->pos >= p->length) { p->failed = 1; return -1; }
            if (p->json[p->pos] == close) { p->pos++; p->tokens[id].end = p->pos; return id; }
            if (p->json[p->pos++] != ',') { p->failed = 1; return -1; } skip_ws(p);
        }
        p->failed = 1; return -1;
    }
    start = p->pos;
    while (p->pos < p->length) { c = p->json[p->pos]; if (c == ',' || c == ']' || c == '}' || isspace((unsigned char)c)) break; p->pos++; }
    if (p->pos == start) { p->failed = 1; return -1; }
    id = add_token(p, CB_JSON_PRIMITIVE, start, parent); if (id < 0) return -1; p->tokens[id].end = p->pos; return id;
}
int cb_json_parse(const char *json, cb_json_token *tokens, int max_tokens) {
    parser p; int root; if (!json || !tokens || max_tokens <= 0) return -1;
    memset(&p, 0, sizeof(p)); p.json = json; p.length = (int)strlen(json); p.tokens = tokens; p.max_tokens = max_tokens;
    skip_ws(&p); root = parse_value(&p, -1, 0); skip_ws(&p);
    if (p.failed || root != 0 || p.pos != p.length) return -1;
    return p.count;
}
static int next_token(const cb_json_token *tokens, int count, int index) {
    int end, i; if (index < 0 || index >= count) return count; end = tokens[index].end;
    for (i = index + 1; i < count && tokens[i].start < end; i++) {} return i;
}
static int token_equals(const char *json, const cb_json_token *token, const char *value) {
    size_t n; if (!json || !token || token->type != CB_JSON_STRING || !value) return 0;
    n = (size_t)(token->end - token->start - 2); return strlen(value) == n && memcmp(json + token->start + 1, value, n) == 0;
}
int cb_json_object_get(const char *json, const cb_json_token *tokens, int count, int object, const char *key) {
    int i; if (!json || !tokens || object < 0 || object >= count || tokens[object].type != CB_JSON_OBJECT) return -1;
    i = object + 1; while (i < count && tokens[i].parent == object) {
        int value = i + 1; if (value >= count || tokens[value].parent != object) return -1;
        if (token_equals(json, &tokens[i], key)) return value;
        i = next_token(tokens, count, value);
    } return -1;
}
int cb_json_array_get(const cb_json_token *tokens, int count, int array, int index) {
    int i, current = 0; if (!tokens || array < 0 || array >= count || tokens[array].type != CB_JSON_ARRAY || index < 0) return -1;
    i = array + 1; while (i < count && tokens[i].parent == array) { if (current == index) return i; i = next_token(tokens, count, i); current++; } return -1;
}
int cb_json_array_size(const cb_json_token *tokens, int count, int array) {
    int i, n = 0; if (!tokens || array < 0 || array >= count || tokens[array].type != CB_JSON_ARRAY) return 0;
    i = array + 1; while (i < count && tokens[i].parent == array) { i = next_token(tokens, count, i); n++; } return n;
}
int cb_json_string(const char *json, const cb_json_token *token, char *out, size_t out_size) {
    size_t src, end, dst = 0; if (!json || !token || !out || out_size == 0 || token->type != CB_JSON_STRING) return -1;
    src = (size_t)token->start + 1; end = (size_t)token->end - 1;
    while (src < end && dst + 1 < out_size) {
        unsigned char c = (unsigned char)json[src++];
        if (c == '\\' && src < end) {
            c = (unsigned char)json[src++];
            switch (c) {
                case '"': out[dst++] = '"'; break; case '\\': out[dst++] = '\\'; break; case '/': out[dst++] = '/'; break;
                case 'b': out[dst++] = '\b'; break; case 'f': out[dst++] = '\f'; break; case 'n': out[dst++] = '\n'; break;
                case 'r': out[dst++] = '\r'; break; case 't': out[dst++] = '\t'; break;
                case 'u': { unsigned int code = 0; int i; for (i = 0; i < 4 && src < end; i++) {
                    unsigned char h = (unsigned char)json[src++]; code <<= 4;
                    if (h >= '0' && h <= '9') code |= h - '0'; else if (h >= 'a' && h <= 'f') code |= h - 'a' + 10; else if (h >= 'A' && h <= 'F') code |= h - 'A' + 10;
                } out[dst++] = code < 0x80 ? (char)code : '?'; break; }
                default: out[dst++] = (char)c; break;
            }
        } else out[dst++] = (char)c;
    }
    out[dst] = '\0'; return (int)dst;
}
int cb_json_int(const char *json, const cb_json_token *token, int fallback) {
    char value[32], *end; long n; size_t len;
    if (!json || !token || token->type != CB_JSON_PRIMITIVE) return fallback;
    len = (size_t)(token->end - token->start); if (len == 0 || len >= sizeof(value)) return fallback;
    memcpy(value, json + token->start, len); value[len] = '\0'; n = strtol(value, &end, 10);
    if (end == value || *end != '\0' || n < -2147483647L || n > 2147483647L) return fallback;
    return (int)n;
}
int cb_json_bool(const char *json, const cb_json_token *token, int fallback) {
    if (!json || !token || token->type != CB_JSON_PRIMITIVE) return fallback;
    if (token->end - token->start == 4 && memcmp(json + token->start, "true", 4) == 0) return 1;
    if (token->end - token->start == 5 && memcmp(json + token->start, "false", 5) == 0) return 0;
    return fallback;
}
