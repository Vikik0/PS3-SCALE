#include "ps3scale/support_payload.h"

#include <string.h>

int ps3scale_support_request_id_valid(const char *value)
{
    size_t i;

    if (!value || strlen(value) != PS3SCALE_SUPPORT_REQUEST_ID_LENGTH)
        return 0;
    for (i = 0; i < PS3SCALE_SUPPORT_REQUEST_ID_LENGTH; i++) {
        unsigned char c = (unsigned char)value[i];
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (c != '-')
                return 0;
        } else if (!((c >= '0' && c <= '9') ||
                     (c >= 'a' && c <= 'f'))) {
            return 0;
        }
    }
    return value[14] == '4' &&
           (value[19] == '8' || value[19] == '9' ||
            value[19] == 'a' || value[19] == 'b');
}

static int append_bytes(char *destination, size_t capacity, size_t *used,
                        const char *value, size_t length)
{
    if (!destination || !used || !value ||
        *used >= capacity || length >= capacity - *used)
        return -1;
    memcpy(destination + *used, value, length);
    *used += length;
    destination[*used] = '\0';
    return 0;
}

static int append_literal(char *destination, size_t capacity, size_t *used,
                          const char *value)
{
    return append_bytes(destination, capacity, used, value, strlen(value));
}

static int append_json_string(char *destination, size_t capacity,
                              size_t *used, const char *value)
{
    const unsigned char *cursor = (const unsigned char *)value;

    if (append_literal(destination, capacity, used, "\"") != 0)
        return -1;
    while (*cursor) {
        char escaped[2];
        const char *replacement = NULL;
        size_t replacement_length = 0;
        unsigned char c = *cursor++;

        switch (c) {
        case '"':  replacement = "\\\""; replacement_length = 2; break;
        case '\\': replacement = "\\\\"; replacement_length = 2; break;
        case '\b': replacement = "\\b"; replacement_length = 2; break;
        case '\f': replacement = "\\f"; replacement_length = 2; break;
        case '\n': replacement = "\\n"; replacement_length = 2; break;
        case '\r': replacement = "\\r"; replacement_length = 2; break;
        case '\t': replacement = "\\t"; replacement_length = 2; break;
        default:
            if (c < 0x20)
                continue;
            escaped[0] = (char)c;
            escaped[1] = '\0';
            replacement = escaped;
            replacement_length = 1;
            break;
        }
        if (append_bytes(destination, capacity, used,
                         replacement, replacement_length) != 0)
            return -1;
    }
    return append_literal(destination, capacity, used, "\"");
}

static int valid_text(const char *value, size_t maximum, int required)
{
    size_t length;
    if (!value)
        return 0;
    length = strlen(value);
    if (required && length == 0)
        return 0;
    return length <= maximum;
}

int ps3scale_support_build_json(
    char *destination,
    size_t capacity,
    const char *request_id,
    const char *kind,
    const char *title,
    const char *details,
    const char *model,
    const char *firmware,
    const char *platform,
    const char *app_version)
{
    size_t used = 0;

    if (!destination || capacity == 0)
        return -1;
    destination[0] = '\0';

    if (!ps3scale_support_request_id_valid(request_id) ||
        !kind || (strcmp(kind, "issue") != 0 &&
                  strcmp(kind, "feedback") != 0) ||
        !valid_text(title, PS3SCALE_SUPPORT_TITLE_MAX, 1) ||
        !valid_text(details, PS3SCALE_SUPPORT_DETAILS_MAX, 1) ||
        !valid_text(model, 80u, 1) ||
        !valid_text(firmware, 40u, 1) ||
        !valid_text(platform, 80u, 1) ||
        !valid_text(app_version, 40u, 1))
        return -1;

    if (append_literal(destination, capacity, &used, "{\"request_id\":") ||
        append_json_string(destination, capacity, &used, request_id) ||
        append_literal(destination, capacity, &used, ",\"kind\":") ||
        append_json_string(destination, capacity, &used, kind) ||
        append_literal(destination, capacity, &used, ",\"title\":") ||
        append_json_string(destination, capacity, &used, title) ||
        append_literal(destination, capacity, &used, ",\"details\":") ||
        append_json_string(destination, capacity, &used, details) ||
        append_literal(destination, capacity, &used, ",\"model\":") ||
        append_json_string(destination, capacity, &used, model) ||
        append_literal(destination, capacity, &used, ",\"firmware\":") ||
        append_json_string(destination, capacity, &used, firmware) ||
        append_literal(destination, capacity, &used, ",\"platform\":") ||
        append_json_string(destination, capacity, &used, platform) ||
        append_literal(destination, capacity, &used, ",\"app_version\":") ||
        append_json_string(destination, capacity, &used, app_version) ||
        append_literal(destination, capacity, &used, "}"))
        return -1;

    return 0;
}
