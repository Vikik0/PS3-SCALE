#include "ps3scale/catalog.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CATALOG_LINE_MAX 1024
#define PROFILE_PATCH_MAX_BYTES 16

static int copy_field(char *destination, size_t capacity, const char *source)
{
    size_t length;

    if (!destination || !capacity || !source)
        return -1;
    length = strlen(source);
    if (!length || length >= capacity)
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

static int valid_source_url(const char *value)
{
    size_t i, length;

    if (!value || strncmp(value, "https://", 8) != 0)
        return 0;
    length = strlen(value);
    if (length <= 8)
        return 0;
    for (i = 0; i < length; i++) {
        unsigned char ch = (unsigned char)value[i];
        if (ch <= 0x20u || ch >= 0x7fu)
            return 0;
    }
    return 1;
}

static int valid_trust(const char *value)
{
    return value &&
           (strcmp(value, "B_COMMUNITY") == 0 ||
            strcmp(value, "B_COMMUNITY_MANUAL") == 0);
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

static int valid_hex(const char *value, size_t max_bytes)
{
    size_t i, length;

    if (!value || !value[0])
        return 0;
    length = strlen(value);
    if ((length & 1u) || length > max_bytes * 2u)
        return 0;
    for (i = 0; i < length; i++) {
        if (!isxdigit((unsigned char)value[i]) ||
            value[i] != (char)toupper((unsigned char)value[i]))
            return 0;
    }
    return 1;
}

static int valid_hex_address(const char *value)
{
    size_t i, length;

    if (!value || !value[0])
        return 0;
    length = strlen(value);
    if (length > 8)
        return 0;
    for (i = 0; i < length; i++) {
        if (!isxdigit((unsigned char)value[i]) ||
            value[i] != (char)toupper((unsigned char)value[i]))
            return 0;
    }
    return 1;
}

static int valid_patch_list(const char *value, unsigned *patch_count)
{
    char copy[PS3SCALE_PROFILE_PATCHES_MAX];
    char *entry;
    unsigned count = 0;

    if (!value || !value[0] || strlen(value) >= sizeof(copy))
        return -1;
    memcpy(copy, value, strlen(value) + 1);

    entry = strtok(copy, ";");
    while (entry) {
        char *first = strchr(entry, ',');
        char *second;
        char *address;
        char *expected;
        char *replacement;
        size_t address_length;
        char *end = NULL;
        unsigned long address_value;

        if (!first)
            return -1;
        *first++ = '\0';
        second = strchr(first, ',');
        if (!second)
            return -1;
        *second++ = '\0';

        address = entry;
        expected = first;
        replacement = second;
        address_length = strlen(address);
        if (!address_length || address_length > 8)
            return -1;
        if (!valid_hex_address(address) ||
            !valid_hex(expected, PROFILE_PATCH_MAX_BYTES) ||
            !valid_hex(replacement, PROFILE_PATCH_MAX_BYTES) ||
            strlen(expected) != strlen(replacement))
            return -1;

        address_value = strtoul(address, &end, 16);
        if (!end || *end || address_value == 0 ||
            address_value > 0xFFFFFFDFul || strcmp(expected, replacement) == 0)
            return -1;

        count++;
        if (count > PS3SCALE_PROFILE_MAX_PATCHES)
            return -1;
        entry = strtok(NULL, ";");
    }

    if (!count)
        return -1;
    if (patch_count)
        *patch_count = count;
    return 0;
}

static int parse_profile_line(char *line, ps3scale_profile_t *profile)
{
    char *fields[8];
    char *cursor = line;
    size_t i;

    if (!line || !profile)
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

    memset(profile, 0, sizeof(*profile));
    if (copy_field(profile->title_id, sizeof(profile->title_id), fields[0]) ||
        copy_field(profile->app_ver, sizeof(profile->app_ver), fields[1]) ||
        copy_field(profile->title, sizeof(profile->title), fields[2]) ||
        copy_field(profile->effect, sizeof(profile->effect), fields[3]) ||
        copy_field(profile->trust, sizeof(profile->trust), fields[4]) ||
        copy_field(profile->patches, sizeof(profile->patches), fields[5]) ||
        copy_field(profile->notes, sizeof(profile->notes), fields[6]) ||
        copy_field(profile->source_url, sizeof(profile->source_url), fields[7]))
        return -1;

    if (!valid_title_id(profile->title_id) ||
        !valid_app_version(profile->app_ver) ||
        !valid_trust(profile->trust) ||
        !valid_source_url(profile->source_url) ||
        valid_patch_list(profile->patches, &profile->patch_count) != 0)
        return -1;

    return 0;
}

int ps3scale_catalog_load(const char *path, ps3scale_catalog_t *catalog)
{
    FILE *file;
    char line[CATALOG_LINE_MAX];
    ps3scale_catalog_t *parsed;

    if (!path || !catalog)
        return -1;

    file = fopen(path, "rb");
    if (!file)
        return -1;

    parsed = (ps3scale_catalog_t *)malloc(sizeof(*parsed));
    if (!parsed) {
        fclose(file);
        return -1;
    }
    memset(parsed, 0, sizeof(*parsed));
    while (fgets(line, sizeof(line), file)) {
        size_t length = strlen(line);
        ps3scale_profile_t profile;
        size_t i;

        if (length && line[length - 1] != '\n' && !feof(file)) {
            fclose(file);
            free(parsed);
            return -1; /* Reject overlong or truncated records. */
        }
        while (length && (line[length - 1] == '\n' ||
                          line[length - 1] == '\r')) {
            line[--length] = '\0';
        }
        if (!length || line[0] == '#')
            continue;

        if (parsed->count >= PS3SCALE_CATALOG_MAX_PROFILES ||
            parse_profile_line(line, &profile) != 0) {
            fclose(file);
            free(parsed);
            return -1;
        }

        for (i = 0; i < parsed->count; i++) {
            if (strcmp(parsed->items[i].title_id, profile.title_id) == 0 &&
                strcmp(parsed->items[i].app_ver, profile.app_ver) == 0) {
                fclose(file);
                free(parsed);
                return -1; /* Duplicate exact identity is ambiguous. */
            }
        }
        parsed->items[parsed->count++] = profile;
    }

    if (ferror(file)) {
        fclose(file);
        free(parsed);
        return -1;
    }
    fclose(file);

    if (!parsed->count) {
        free(parsed);
        return -1;
    }

    *catalog = *parsed;
    {
        int count = (int)parsed->count;
        free(parsed);
        return count;
    }
}

const ps3scale_profile_t *ps3scale_catalog_match(
    const ps3scale_catalog_t *catalog,
    const char *title_id,
    const char *app_ver)
{
    size_t i;

    if (!catalog || !title_id || !app_ver ||
        !valid_title_id(title_id) || !valid_app_version(app_ver))
        return NULL;

    for (i = 0; i < catalog->count; i++) {
        const ps3scale_profile_t *profile = &catalog->items[i];
        if (strcmp(profile->title_id, title_id) == 0 &&
            strcmp(profile->app_ver, app_ver) == 0)
            return profile;
    }
    return NULL;
}

const ps3scale_profile_t *ps3scale_catalog_match_game(
    const ps3scale_catalog_t *catalog,
    const ps3scale_game_t *game)
{
    if (!game)
        return NULL;
    return ps3scale_catalog_match(catalog, game->title_id, game->app_ver);
}

size_t ps3scale_catalog_count_matches(
    const ps3scale_catalog_t *catalog,
    const ps3scale_game_list_t *games)
{
    size_t i, count = 0;

    if (!catalog || !games)
        return 0;

    for (i = 0; i < games->count; i++) {
        const ps3scale_game_t *game = &games->items[i];
        if (game->kind == PS3SCALE_GAME_PS3_FOLDER ||
            game->kind == PS3SCALE_GAME_PS3_HDD ||
            game->kind == PS3SCALE_GAME_PS3_ISO) {
            if (ps3scale_catalog_match_game(catalog, game))
                count++;
        }
    }
    return count;
}
