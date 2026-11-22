#include "online.h"
#include "json_min.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef CB_CA_BUNDLE
#define CB_CA_BUNDLE "/dev_hdd0/game/CBON00001/USRDIR/cacert.pem"
#endif
#define CB_HTTP_BUFFER 16384
#define CB_JSON_TOKEN_COUNT 1024
static int g_curl_ready = 0;
typedef struct { char data[CB_HTTP_BUFFER]; size_t length; } response_buffer;
static size_t write_response(void *contents, size_t size, size_t count, void *user_data) {
    response_buffer *buffer = (response_buffer *)user_data; size_t bytes = size * count;
    if (!buffer || bytes > sizeof(buffer->data) - buffer->length - 1) return 0;
    memcpy(buffer->data + buffer->length, contents, bytes); buffer->length += bytes; buffer->data[buffer->length] = '\0'; return bytes;
}
static void set_error(char *error, size_t size, const char *message) { if (error && size) snprintf(error, size, "%s", message ? message : "Unknown lobby error"); }
static int parse_error(const char *body, char *error, size_t size, long status) {
    cb_json_token tokens[CB_JSON_TOKEN_COUNT]; int count = cb_json_parse(body, tokens, CB_JSON_TOKEN_COUNT);
    int item = count > 0 ? cb_json_object_get(body, tokens, count, 0, "error") : -1;
    if (item >= 0 && tokens[item].type == CB_JSON_STRING) cb_json_string(body, &tokens[item], error, size);
    else if (error && size) snprintf(error, size, "Lobby request failed (HTTP %ld)", status);
    return -1;
}
static int http_request(const char *method, const char *path, const char *body, const char *token,
                        char *output, size_t output_size, char *error, size_t error_size) {
    CURL *curl; CURLcode result; struct curl_slist *headers = NULL; response_buffer response;
    char url[512], auth[CB_ONLINE_TOKEN_MAX + 32]; long status = 0;
    if (!g_curl_ready) { set_error(error, error_size, "HTTPS client is not initialized"); return -1; }
    if (!method || !path || !output || output_size == 0) { set_error(error, error_size, "Invalid lobby request"); return -1; }
    memset(&response, 0, sizeof(response)); snprintf(url, sizeof(url), "%s%s", CB_ONLINE_API_BASE, path);
    curl = curl_easy_init(); if (!curl) { set_error(error, error_size, "Could not create HTTPS request"); return -1; }
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "CrashBashEnhanced-PS3/0.3");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 5000L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 9000L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_CAINFO, CB_CA_BUNDLE);
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "https");
#else
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);
#endif
#ifdef CURL_SSLVERSION_TLSv1_2
    curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
#endif
    if (token && token[0]) { snprintf(auth, sizeof(auth), "Authorization: Bearer %s", token); headers = curl_slist_append(headers, auth); }
    if (strcmp(method, "POST") == 0) {
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body ? body : "{}");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(body ? body : "{}"));
    } else curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    result = curl_easy_perform(curl); curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    if (result != CURLE_OK) {
        if (error && error_size) snprintf(error, error_size, "HTTPS error: %s", curl_easy_strerror(result));
        curl_slist_free_all(headers); curl_easy_cleanup(curl); return -1;
    }
    if (status < 200 || status >= 300) { parse_error(response.data, error, error_size, status); curl_slist_free_all(headers); curl_easy_cleanup(curl); return -1; }
    if (response.length + 1 > output_size) { set_error(error, error_size, "Lobby response buffer is too small"); curl_slist_free_all(headers); curl_easy_cleanup(curl); return -1; }
    memcpy(output, response.data, response.length + 1); curl_slist_free_all(headers); curl_easy_cleanup(curl);
    if (error && error_size) error[0] = '\0'; return 0;
}
static int get_string_field(const char *json, const cb_json_token *tokens, int count, int object, const char *key, char *out, size_t size) {
    int value = cb_json_object_get(json, tokens, count, object, key);
    if (value < 0 || tokens[value].type != CB_JSON_STRING) return -1;
    return cb_json_string(json, &tokens[value], out, size);
}
static void parse_players(const char *json, const cb_json_token *tokens, int count, int array, CBOnlineRoomView *view) {
    int i, n; if (!view) return; view->player_count = 0;
    if (array < 0 || tokens[array].type != CB_JSON_ARRAY) return;
    n = cb_json_array_size(tokens, count, array); if (n > CB_ONLINE_PLAYER_MAX) n = CB_ONLINE_PLAYER_MAX;
    for (i = 0; i < n; i++) {
        int item = cb_json_array_get(tokens, count, array, i);
        int ready_token; CBOnlinePlayer *player = &view->players[view->player_count];
        memset(player, 0, sizeof(*player));
        get_string_field(json, tokens, count, item, "id", player->id, sizeof(player->id));
        get_string_field(json, tokens, count, item, "name", player->name, sizeof(player->name));
        ready_token = cb_json_object_get(json, tokens, count, item, "ready");
        player->ready = cb_json_bool(json, ready_token >= 0 ? &tokens[ready_token] : NULL, 0);
        view->player_count++;
    }
    view->is_ready = view->player_count == CB_ONLINE_PLAYER_MAX;
    for (i = 0; i < view->player_count; i++) if (!view->players[i].ready) view->is_ready = 0;
}
static int parse_room_view(const char *json, CBOnlineRoomView *view) {
    cb_json_token tokens[CB_JSON_TOKEN_COUNT]; int count = cb_json_parse(json, tokens, CB_JSON_TOKEN_COUNT);
    int room, players, status, private_token;
    if (count <= 0 || !view) return -1;
    room = cb_json_object_get(json, tokens, count, 0, "room");
    if (room < 0 || tokens[room].type != CB_JSON_OBJECT) return -1;
    memset(view, 0, sizeof(*view));
    get_string_field(json, tokens, count, room, "code", view->code, sizeof(view->code));
    status = cb_json_object_get(json, tokens, count, room, "status");
    if (status >= 0 && tokens[status].type == CB_JSON_STRING) {
        char status_text[16]; cb_json_string(json, &tokens[status], status_text, sizeof(status_text));
        view->is_ready = strcmp(status_text, "ready") == 0;
    }
    private_token = cb_json_object_get(json, tokens, count, room, "isPrivate");
    view->is_private = cb_json_bool(json, private_token >= 0 ? &tokens[private_token] : NULL, 0);
    players = cb_json_object_get(json, tokens, count, room, "players");
    parse_players(json, tokens, count, players, view);
    return 0;
}
static int parse_session(const char *json, CBOnlineSession *session) {
    cb_json_token tokens[CB_JSON_TOKEN_COUNT]; int count = cb_json_parse(json, tokens, CB_JSON_TOKEN_COUNT);
    int room, player, token;
    if (count <= 0 || !session) return -1;
    room = cb_json_object_get(json, tokens, count, 0, "room");
    player = cb_json_object_get(json, tokens, count, 0, "player");
    token = cb_json_object_get(json, tokens, count, 0, "token");
    if (room < 0 || player < 0 || token < 0) return -1;
    memset(session, 0, sizeof(*session));
    if (get_string_field(json, tokens, count, room, "code", session->code, sizeof(session->code)) < 0) return -1;
    if (get_string_field(json, tokens, count, player, "id", session->player_id, sizeof(session->player_id)) < 0) return -1;
    if (get_string_field(json, tokens, count, player, "name", session->player_name, sizeof(session->player_name)) < 0) return -1;
    return cb_json_string(json, &tokens[token], session->token, sizeof(session->token)) < 0 ? -1 : 0;
}
static int parse_heartbeat(const char *json, CBOnlineRoomView *view) {
    cb_json_token tokens[CB_JSON_TOKEN_COUNT]; int count = cb_json_parse(json, tokens, CB_JSON_TOKEN_COUNT);
    int players; if (count <= 0 || !view) return -1;
    players = cb_json_object_get(json, tokens, count, 0, "players");
    parse_players(json, tokens, count, players, view); return 0;
}
static void json_escape(const char *input, char *output, size_t size) {
    size_t i, used = 0; if (!output || size == 0) return; output[used++] = '"';
    if (input) for (i = 0; input[i] && used + 7 < size; i++) {
        unsigned char c = (unsigned char)input[i];
        if (c == '"' || c == '\\') { output[used++] = '\\'; output[used++] = (char)c; }
        else if (c == '\n') { output[used++] = '\\'; output[used++] = 'n'; }
        else if (c == '\r') { output[used++] = '\\'; output[used++] = 'r'; }
        else if (c == '\t') { output[used++] = '\\'; output[used++] = 't'; }
        else if (c >= 0x20) output[used++] = (char)c;
    }
    output[used++] = '"'; output[used] = '\0';
}
int cb_online_init(void) { CURLcode rc; if (g_curl_ready) return 0; rc = curl_global_init(CURL_GLOBAL_DEFAULT); if (rc != CURLE_OK) return -1; g_curl_ready = 1; return 0; }
void cb_online_shutdown(void) { if (g_curl_ready) { curl_global_cleanup(); g_curl_ready = 0; } }
int cb_online_health(char *error, size_t error_size) {
    char response[CB_HTTP_BUFFER]; cb_json_token tokens[CB_JSON_TOKEN_COUNT]; int count, ok, storage, secret;
    if (http_request("GET", "/api/health", NULL, NULL, response, sizeof(response), error, error_size) < 0) return -1;
    count = cb_json_parse(response, tokens, CB_JSON_TOKEN_COUNT); if (count <= 0) { set_error(error, error_size, "Invalid health response"); return -1; }
    ok = cb_json_object_get(response, tokens, count, 0, "ok");
    storage = cb_json_object_get(response, tokens, count, 0, "storageConfigured");
    secret = cb_json_object_get(response, tokens, count, 0, "tokenConfigured");
    if (!cb_json_bool(response, ok >= 0 ? &tokens[ok] : NULL, 0) ||
        !cb_json_bool(response, storage >= 0 ? &tokens[storage] : NULL, 0) ||
        !cb_json_bool(response, secret >= 0 ? &tokens[secret] : NULL, 0)) {
        set_error(error, error_size, "Lobby API is reachable but cloud storage or guest-token setup is missing"); return -1;
    }
    if (error && error_size) error[0] = '\0'; return 0;
}
int cb_online_list_rooms(CBOnlineRoom *rooms, size_t capacity, size_t *count_out, char *error, size_t error_size) {
    char response[CB_HTTP_BUFFER]; cb_json_token tokens[CB_JSON_TOKEN_COUNT]; int count, array, n, i; size_t written = 0;
    if (count_out) *count_out = 0;
    if (!rooms || capacity == 0 || !count_out) { set_error(error, error_size, "Invalid room list buffer"); return -1; }
    if (http_request("GET", "/api/rooms", NULL, NULL, response, sizeof(response), error, error_size) < 0) return -1;
    count = cb_json_parse(response, tokens, CB_JSON_TOKEN_COUNT); if (count <= 0) { set_error(error, error_size, "Invalid room list response"); return -1; }
    array = cb_json_object_get(response, tokens, count, 0, "rooms");
    if (array < 0 || tokens[array].type != CB_JSON_ARRAY) { set_error(error, error_size, "Room list is missing"); return -1; }
    n = cb_json_array_size(tokens, count, array);
    for (i = 0; i < n && written < capacity; i++) {
        int item = cb_json_array_get(tokens, count, array, i);
        int players, max_players; CBOnlineRoom *room = &rooms[written]; memset(room, 0, sizeof(*room));
        get_string_field(response, tokens, count, item, "code", room->code, sizeof(room->code));
        get_string_field(response, tokens, count, item, "host", room->host, sizeof(room->host));
        players = cb_json_object_get(response, tokens, count, item, "players");
        max_players = cb_json_object_get(response, tokens, count, item, "maxPlayers");
        room->players = cb_json_int(response, players >= 0 ? &tokens[players] : NULL, 0);
        room->max_players = cb_json_int(response, max_players >= 0 ? &tokens[max_players] : NULL, CB_ONLINE_PLAYER_MAX);
        if (room->code[0]) written++;
    }
    *count_out = written; if (error && error_size) error[0] = '\0'; return 0;
}
static int create_or_join(const char *action, const char *code, const char *display_name, int is_private,
                          CBOnlineSession *session, CBOnlineRoomView *view, char *error, size_t error_size) {
    char escaped_name[CB_ONLINE_NAME_MAX * 6 + 4], body[512], response[CB_HTTP_BUFFER];
    if (!session || !view) { set_error(error, error_size, "Invalid online session buffer"); return -1; }
    json_escape(display_name && display_name[0] ? display_name : "Guest", escaped_name, sizeof(escaped_name));
    if (strcmp(action, "create") == 0)
        snprintf(body, sizeof(body), "{\"action\":\"create\",\"displayName\":%s,\"isPrivate\":%s}", escaped_name, is_private ? "true" : "false");
    else snprintf(body, sizeof(body), "{\"action\":\"join\",\"code\":\"%.6s\",\"displayName\":%s}", code ? code : "", escaped_name);
    if (http_request("POST", "/api/rooms", body, NULL, response, sizeof(response), error, error_size) < 0) return -1;
    if (parse_session(response, session) < 0) { set_error(error, error_size, "Lobby did not return a valid guest session"); return -1; }
    memset(view, 0, sizeof(*view)); snprintf(view->code, sizeof(view->code), "%s", session->code);
    if (cb_online_get_room(session, view, error, error_size) < 0) {
        snprintf(view->players[0].id, sizeof(view->players[0].id), "%s", session->player_id);
        snprintf(view->players[0].name, sizeof(view->players[0].name), "%s", session->player_name);
        view->player_count = 1; if (error && error_size) error[0] = '\0';
    }
    return 0;
}
int cb_online_create_room(const char *display_name, int is_private, CBOnlineSession *session, CBOnlineRoomView *view, char *error, size_t error_size) {
    return create_or_join("create", NULL, display_name, is_private, session, view, error, error_size);
}
int cb_online_join_room(const char *code, const char *display_name, CBOnlineSession *session, CBOnlineRoomView *view, char *error, size_t error_size) {
    if (!code || strlen(code) != 6) { set_error(error, error_size, "Room code must contain six characters"); return -1; }
    return create_or_join("join", code, display_name, 0, session, view, error, error_size);
}
int cb_online_join_any_room(const char *display_name, CBOnlineSession *session, CBOnlineRoomView *view, char *error, size_t error_size) {
    char escaped_name[CB_ONLINE_NAME_MAX * 6 + 4], response[CB_HTTP_BUFFER], body[256];
    if (!session || !view) { set_error(error, error_size, "Invalid online session buffer"); return -1; }
    json_escape(display_name && display_name[0] ? display_name : "Guest", escaped_name, sizeof(escaped_name));
    snprintf(body, sizeof(body), "{\"action\":\"any\",\"displayName\":%s}", escaped_name);
    if (http_request("POST", "/api/rooms", body, NULL, response, sizeof(response), error, error_size) < 0) return -1;
    if (parse_session(response, session) < 0) { set_error(error, error_size, "Lobby did not return a valid public-room session"); return -1; }
    memset(view, 0, sizeof(*view));
    snprintf(view->code, sizeof(view->code), "%s", session->code);
    if (cb_online_get_room(session, view, error, error_size) < 0) {
        snprintf(view->players[0].id, sizeof(view->players[0].id), "%s", session->player_id);
        snprintf(view->players[0].name, sizeof(view->players[0].name), "%s", session->player_name);
        view->player_count = 1;
        if (error && error_size) error[0] = '\0';
    }
    return 0;
}
int cb_online_get_room(CBOnlineSession *session, CBOnlineRoomView *view, char *error, size_t error_size) {
    char path[64], response[CB_HTTP_BUFFER]; int i;
    if (!session || !session->token[0] || !session->code[0] || !view) { set_error(error, error_size, "No active room session"); return -1; }
    snprintf(path, sizeof(path), "/api/rooms?code=%.6s", session->code);
    if (http_request("GET", path, NULL, session->token, response, sizeof(response), error, error_size) < 0) return -1;
    if (parse_room_view(response, view) < 0) { set_error(error, error_size, "Invalid room details response"); return -1; }
    for (i = 0; i < view->player_count; i++) if (strcmp(view->players[i].id, session->player_id) == 0) session->ready = view->players[i].ready;
    return 0;
}
int cb_online_set_ready(CBOnlineSession *session, int ready, CBOnlineRoomView *view, char *error, size_t error_size) {
    char body[64], response[CB_HTTP_BUFFER];
    if (!session || !session->token[0] || !view) { set_error(error, error_size, "No active room session"); return -1; }
    snprintf(body, sizeof(body), "{\"action\":\"heartbeat\",\"ready\":%s}", ready ? "true" : "false");
    if (http_request("POST", "/api/rooms", body, session->token, response, sizeof(response), error, error_size) < 0) return -1;
    if (parse_heartbeat(response, view) < 0) { set_error(error, error_size, "Invalid room heartbeat response"); return -1; }
    snprintf(view->code, sizeof(view->code), "%s", session->code); session->ready = ready; return 0;
}
int cb_online_leave_room(CBOnlineSession *session, char *error, size_t error_size) {
    char response[CB_HTTP_BUFFER]; if (!session || !session->token[0]) return 0;
    if (http_request("POST", "/api/rooms", "{\"action\":\"leave\"}", session->token, response, sizeof(response), error, error_size) < 0) return -1;
    memset(session, 0, sizeof(*session)); if (error && error_size) error[0] = '\0'; return 0;
}
