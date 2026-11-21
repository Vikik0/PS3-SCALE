#ifndef CB_JSON_MIN_H
#define CB_JSON_MIN_H
#include <stddef.h>
typedef enum { CB_JSON_OBJECT = 1, CB_JSON_ARRAY, CB_JSON_STRING, CB_JSON_PRIMITIVE } cb_json_type;
typedef struct { cb_json_type type; int start; int end; int parent; } cb_json_token;
int cb_json_parse(const char *json, cb_json_token *tokens, int max_tokens);
int cb_json_object_get(const char *json, const cb_json_token *tokens, int count, int object, const char *key);
int cb_json_array_get(const cb_json_token *tokens, int count, int array, int index);
int cb_json_array_size(const cb_json_token *tokens, int count, int array);
int cb_json_string(const char *json, const cb_json_token *token, char *out, size_t out_size);
int cb_json_int(const char *json, const cb_json_token *token, int fallback);
int cb_json_bool(const char *json, const cb_json_token *token, int fallback);
#endif
