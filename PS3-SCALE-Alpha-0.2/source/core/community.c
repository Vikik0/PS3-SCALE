#include "ps3scale/community.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define COMMUNITY_LINE_MAX 1024
#define COMMUNITY_FILE_MAX_BYTES (1024ul * 1024ul)

static int copy_field(char *destination, size_t capacity,
                      const char *source, int allow_empty)
{
    size_t length;
    if (!destination || !capacity || !source)
        return -1;
    length = strlen(source);
    if ((!allow_empty && !length) || length >= capacity)
        return -1;
    memcpy(destination, source, length + 1);
    return 0;
}

static int valid_title_id(const char *value)
{
    size_t i;
    if (!value || strlen(value) != 9)
        return 0;
    for (i = 0; i < 9; i++) {
        if (!((value[i] >= 'A' && value[i] <= 'Z') ||
              (value[i] >= '0' && value[i] <= '9')))
            return 0;
    }
    return 1;
}

static int valid_app_version(const char *value)
{
    return value && strlen(value) == 5 &&
           isdigit((unsigned char)value[0]) &&
           isdigit((unsigned char)value[1]) &&
           value[2] == '.' &&
           isdigit((unsigned char)value[3]) &&
           isdigit((unsigned char)value[4]);
}

static int valid_source_kind(const char *value)
{
    return value &&
           (strcmp(value, "HARDWARE_TESTED_FPS") == 0 ||
            strcmp(value, "COMMUNITY_CONFIRMED") == 0 ||
            strcmp(value, "COMMUNITY_UNVERIFIED") == 0 ||
            strcmp(value, "ARTEMIS_DATABASE") == 0 ||
            strcmp(value, "COMMUNITY_CHEATS") == 0);
}

static int valid_repository(const char *value)
{
    const char *slash;
    const char *p;
    if (!value || !value[0])
        return 0;
    slash = strchr(value, '/');
    if (!slash || slash == value || !slash[1] || strchr(slash + 1, '/'))
        return 0;
    for (p = value; *p; p++) {
        unsigned char ch = (unsigned char)*p;
        if (ch == '/')
            continue;
        if (!(isalnum(ch) || ch == '-' || ch == '_' || ch == '.'))
            return 0;
    }
    return 1;
}

static int valid_branch(const char *value)
{
    const char *p;
    size_t length;
    if (!value || !value[0] || value[0] == '/' || strstr(value, ".."))
        return 0;
    length = strlen(value);
    if (value[length - 1] == '/')
        return 0;
    for (p = value; *p; p++) {
        unsigned char ch = (unsigned char)*p;
        if (!(isalnum(ch) || ch == '-' || ch == '_' ||
              ch == '.' || ch == '/'))
            return 0;
    }
    return 1;
}

static int valid_path(const char *value)
{
    size_t length, i;
    if (!value || !value[0] || value[0] == '/' ||
        strcmp(value, "..") == 0 || strstr(value, "../") ||
        strstr(value, "/..") || strchr(value, '\\'))
        return 0;
    length = strlen(value);
    if (length < 5 || length >= PS3SCALE_COMMUNITY_PATH_MAX)
        return 0;
    if (strcasecmp(value + length - 4, ".ncl") != 0)
        return 0;
    for (i = 0; i < length; i++) {
        unsigned char ch = (unsigned char)value[i];
        if (ch < 0x20u || ch == 0x7fu)
            return 0;
    }
    return 1;
}

static int parse_file_size(const char *value, unsigned long *out)
{
    char *end = NULL;
    unsigned long parsed;
    if (!value || !value[0] || !out)
        return -1;
    parsed = strtoul(value, &end, 10);
    if (!end || *end || parsed == 0 ||
        parsed > COMMUNITY_FILE_MAX_BYTES)
        return -1;
    *out = parsed;
    return 0;
}

static int parse_community_line(char *line,
                                ps3scale_community_file_t *record)
{
    char *fields[8];
    char *cursor = line;
    size_t i;
    if (!line || !record)
        return -1;

    for (i = 0; i < 7; i++) {
        char *tab;
        fields[i] = cursor;
        tab = strchr(cursor, '\t');
        if (!tab)
            return -1;
        *tab = '\0';
        cursor = tab + 1;
    }
    fields[7] = cursor;
    if (strchr(fields[7], '\t'))
        return -1;

    memset(record, 0, sizeof(*record));
    if (copy_field(record->title_id, sizeof(record->title_id), fields[0], 1) ||
        copy_field(record->app_ver, sizeof(record->app_ver), fields[1], 1) ||
        copy_field(record->title, sizeof(record->title), fields[2], 1) ||
        copy_field(record->source_kind, sizeof(record->source_kind), fields[3], 0) ||
        copy_field(record->repository, sizeof(record->repository), fields[4], 0) ||
        copy_field(record->branch, sizeof(record->branch), fields[5], 0) ||
        copy_field(record->path, sizeof(record->path), fields[6], 0) ||
        parse_file_size(fields[7], &record->file_size) != 0)
        return -1;

    if ((record->title_id[0] && !valid_title_id(record->title_id)) ||
        (record->app_ver[0] && !valid_app_version(record->app_ver)) ||
        (!record->title_id[0] && !record->title[0]) ||
        !valid_source_kind(record->source_kind) ||
        !valid_repository(record->repository) ||
        !valid_branch(record->branch) ||
        !valid_path(record->path))
        return -1;
    return 0;
}

int ps3scale_community_index_load(
    const char *path,
    ps3scale_community_index_t *index)
{
    FILE *file;
    char line[COMMUNITY_LINE_MAX];
    ps3scale_community_file_t *parsed;
    size_t count = 0;

    if (!path || !index)
        return -1;
    file = fopen(path, "rb");
    if (!file)
        return -1;

    parsed = (ps3scale_community_file_t *)calloc(
        PS3SCALE_COMMUNITY_MAX_FILES, sizeof(*parsed));
    if (!parsed) {
        fclose(file);
        return -1;
    }

    while (fgets(line, sizeof(line), file)) {
        size_t length = strlen(line);
        ps3scale_community_file_t record;
        if (length && line[length - 1] != '\n' && !feof(file)) {
            fclose(file);
            free(parsed);
            return -1;
        }
        while (length && (line[length - 1] == '\n' ||
                          line[length - 1] == '\r'))
            line[--length] = '\0';
        if (!length || line[0] == '#')
            continue;
        if (count >= PS3SCALE_COMMUNITY_MAX_FILES ||
            parse_community_line(line, &record) != 0) {
            fclose(file);
            free(parsed);
            return -1;
        }
        parsed[count++] = record;
    }

    if (ferror(file)) {
        fclose(file);
        free(parsed);
        return -1;
    }
    fclose(file);
    if (!count) {
        free(parsed);
        return -1;
    }

    {
        ps3scale_community_file_t *smaller =
            (ps3scale_community_file_t *)realloc(
                parsed, count * sizeof(*parsed));
        if (smaller)
            parsed = smaller;
    }
    free(index->items);
    index->items = parsed;
    index->count = count;
    return (int)count;
}

void ps3scale_community_index_free(ps3scale_community_index_t *index)
{
    if (!index)
        return;
    free(index->items);
    index->items = NULL;
    index->count = 0;
}

static size_t normalize_title(const char *source,
                              char *destination, size_t capacity)
{
    size_t input, output = 0;
    int pending_space = 0;
    if (!source || !destination || capacity < 2)
        return 0;

    for (input = 0; source[input] && output + 1 < capacity; input++) {
        unsigned char ch = (unsigned char)source[input];
        if (ch < 0x80u && isalnum(ch)) {
            if (pending_space && output + 1 < capacity)
                destination[output++] = ' ';
            destination[output++] = (char)tolower(ch);
            pending_space = 0;
        } else if (output) {
            pending_space = 1;
        }
    }
    destination[output] = '\0';
    return output;
}

static size_t split_tokens(char *value, char **tokens, size_t capacity)
{
    size_t count = 0;
    char *cursor = value;
    while (*cursor && count < capacity) {
        while (*cursor == ' ')
            cursor++;
        if (!*cursor)
            break;
        tokens[count++] = cursor;
        while (*cursor && *cursor != ' ')
            cursor++;
        if (*cursor)
            *cursor++ = '\0';
    }
    return count;
}

static int titles_match(const char *game_title, const char *file_title)
{
    char game_buffer[PS3SCALE_COMMUNITY_TITLE_MAX * 2];
    char file_buffer[PS3SCALE_COMMUNITY_TITLE_MAX * 2];
    char *game_tokens[32];
    char *file_tokens[32];
    size_t game_length, file_length, game_count, file_count;
    size_t i, j, matched = 0;

    game_length = normalize_title(game_title, game_buffer, sizeof(game_buffer));
    file_length = normalize_title(file_title, file_buffer, sizeof(file_buffer));
    if (game_length < 3 || file_length < 3)
        return 0;

    game_count = split_tokens(game_buffer, game_tokens, 32);
    file_count = split_tokens(file_buffer, file_tokens, 32);
    if (!game_count || !file_count)
        return 0;

    for (i = 0; i < game_count; i++) {
        for (j = 0; j < file_count; j++) {
            if (strcmp(game_tokens[i], file_tokens[j]) == 0) {
                matched++;
                break;
            }
        }
    }
    return matched == (game_count < file_count ? game_count : file_count);
}

static int is_ps3_game(ps3scale_game_kind_t kind)
{
    return kind == PS3SCALE_GAME_PS3_FOLDER ||
           kind == PS3SCALE_GAME_PS3_HDD ||
           kind == PS3SCALE_GAME_PS3_ISO;
}

int ps3scale_community_file_matches_game(
    const ps3scale_community_file_t *file,
    const ps3scale_game_t *game)
{
    if (!file || !game || !is_ps3_game(game->kind))
        return 0;
    if (file->title_id[0] && game->title_id[0] &&
        strcmp(file->title_id, game->title_id) == 0)
        return 1;
    return titles_match(game->title, file->title);
}

const char *ps3scale_community_source_label(const char *source_kind)
{
    if (!source_kind)
        return "COMMUNITY FILE";
    if (strcmp(source_kind, "HARDWARE_TESTED_FPS") == 0)
        return "HARDWARE-TESTED FPS PATCH";
    if (strcmp(source_kind, "COMMUNITY_CONFIRMED") == 0)
        return "COMMUNITY-CONFIRMED PATCH";
    if (strcmp(source_kind, "COMMUNITY_UNVERIFIED") == 0)
        return "UNVERIFIED COMMUNITY FILE";
    if (strcmp(source_kind, "ARTEMIS_DATABASE") == 0)
        return "ARTEMIS DATABASE FILE";
    if (strcmp(source_kind, "COMMUNITY_CHEATS") == 0)
        return "COMMUNITY CHEAT FILE";
    return "COMMUNITY FILE";
}
