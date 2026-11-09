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

static int valid_category(const char *value)
{
    return value &&
        (strcmp(value, "GRAPHICS_FPS") == 0 ||
         strcmp(value, "GAMEPLAY_CHEAT") == 0 ||
         strcmp(value, "GAMEPLAY_CODE") == 0 ||
         strcmp(value, "COMMUNITY_CODE") == 0);
}

static int valid_description(const char *value)
{
    size_t i;
    if (!value || !value[0])
        return 0;
    for (i = 0; value[i]; i++) {
        unsigned char ch = (unsigned char)value[i];
        if (ch < 0x20u || ch == 0x7fu)
            return 0;
    }
    return 1;
}

static void set_legacy_metadata(ps3scale_community_file_t *record)
{
    if (strcmp(record->source_kind, "HARDWARE_TESTED_FPS") == 0) {
        strcpy(record->category, "GRAPHICS_FPS");
        strcpy(record->description,
               "FPS patch; exact effect is not documented in this index.");
    } else if (strcmp(record->source_kind, "COMMUNITY_CHEATS") == 0) {
        strcpy(record->category, "GAMEPLAY_CHEAT");
        strcpy(record->description,
               "Gameplay cheat code; not a graphics enhancement.");
    } else if (strcmp(record->source_kind, "ARTEMIS_DATABASE") == 0) {
        strcpy(record->category, "GAMEPLAY_CODE");
        strcpy(record->description,
               "Artemis code entry; not verified as a graphics enhancement.");
    } else {
        strcpy(record->category, "COMMUNITY_CODE");
        strcpy(record->description,
               "Community NCL code; graphics effect is not confirmed.");
    }
}

static int parse_community_line(char *line,
                                ps3scale_community_file_t *record)
{
    char *fields[10];
    char *cursor;
    size_t field_count = 1;

    if (!line || !record)
        return -1;
    fields[0] = line;
    cursor = line;
    while (strchr(cursor, '\t')) {
        char *tab = strchr(cursor, '\t');
        if (field_count >= 10)
            return -1;
        *tab = '\0';
        cursor = tab + 1;
        fields[field_count++] = cursor;
    }
    if (field_count != 8 && field_count != 10)
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

    if (field_count == 10) {
        if (copy_field(record->category, sizeof(record->category), fields[8], 0) ||
            copy_field(record->description, sizeof(record->description), fields[9], 0))
            return -1;
    } else {
        /* Continue accepting the previous eight-column cache until it refreshes. */
        set_legacy_metadata(record);
    }

    if ((record->title_id[0] && !valid_title_id(record->title_id)) ||
        (record->app_ver[0] && !valid_app_version(record->app_ver)) ||
        (!record->title_id[0] && !record->title[0]) ||
        !valid_source_kind(record->source_kind) ||
        !valid_category(record->category) ||
        !valid_description(record->description) ||
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

static int ignored_title_word(const char *word)
{
    return strcmp(word, "the") == 0 ||
           strcmp(word, "of") == 0 ||
           strcmp(word, "and") == 0;
}

static size_t canonical_title_tokens(char *buffer,
                                     char **tokens,
                                     size_t capacity)
{
    char *raw[32];
    size_t raw_count = split_tokens(buffer, raw, 32);
    size_t count = 0, i;

    for (i = 0; i < raw_count && count < capacity; i++) {
        if (!ignored_title_word(raw[i]))
            tokens[count++] = raw[i];
    }

    /* Remove common edition labels, but never drop a numbered sequel. */
    if (count >= 2 && strcmp(tokens[count - 1], "edition") == 0) {
        const char *qualifier = tokens[count - 2];
        if (strcmp(qualifier, "ultimate") == 0 ||
            strcmp(qualifier, "complete") == 0 ||
            strcmp(qualifier, "definitive") == 0 ||
            strcmp(qualifier, "special") == 0 ||
            strcmp(qualifier, "deluxe") == 0 ||
            strcmp(qualifier, "collector") == 0 ||
            strcmp(qualifier, "gold") == 0 ||
            strcmp(qualifier, "platinum") == 0)
            count -= 2;
        else
            count--;
    }
    if (count && (strcmp(tokens[count - 1], "goty") == 0 ||
                  strcmp(tokens[count - 1], "remastered") == 0))
        count--;
    if (count >= 2 && strcmp(tokens[count - 1], "year") == 0 &&
        strcmp(tokens[count - 2], "game") == 0)
        count -= 2;

    return count;
}

static int titles_match(const char *game_title, const char *file_title)
{
    char game_buffer[PS3SCALE_COMMUNITY_TITLE_MAX * 2];
    char file_buffer[PS3SCALE_COMMUNITY_TITLE_MAX * 2];
    char *game_tokens[32];
    char *file_tokens[32];
    size_t game_length, file_length, game_count, file_count, i;

    game_length = normalize_title(game_title, game_buffer, sizeof(game_buffer));
    file_length = normalize_title(file_title, file_buffer, sizeof(file_buffer));
    if (game_length < 3 || file_length < 3)
        return 0;

    game_count = canonical_title_tokens(game_buffer, game_tokens, 32);
    file_count = canonical_title_tokens(file_buffer, file_tokens, 32);
    if (!game_count || game_count != file_count)
        return 0;

    /* Ordered equality prevents sequel/subtitle false positives. */
    for (i = 0; i < game_count; i++) {
        if (strcmp(game_tokens[i], file_tokens[i]) != 0)
            return 0;
    }
    return 1;
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

static int title_region(const char *title_id)
{
    if (!title_id || strlen(title_id) < 4)
        return 0;
    if (strncmp(title_id, "BLUS", 4) == 0 ||
        strncmp(title_id, "BCUS", 4) == 0 ||
        strncmp(title_id, "NPUB", 4) == 0 ||
        strncmp(title_id, "NPUS", 4) == 0)
        return 1; /* North America / US */
    if (strncmp(title_id, "BLES", 4) == 0 ||
        strncmp(title_id, "BCES", 4) == 0 ||
        strncmp(title_id, "NPEB", 4) == 0 ||
        strncmp(title_id, "NPED", 4) == 0)
        return 2; /* Europe */
    if (strncmp(title_id, "BLJM", 4) == 0 ||
        strncmp(title_id, "BCJS", 4) == 0 ||
        strncmp(title_id, "NPJB", 4) == 0 ||
        strncmp(title_id, "NPJJ", 4) == 0 ||
        strncmp(title_id, "NPJA", 4) == 0)
        return 3; /* Japan */
    if (strncmp(title_id, "BCAS", 4) == 0 ||
        strncmp(title_id, "BLAS", 4) == 0 ||
        strncmp(title_id, "NPHB", 4) == 0 ||
        strncmp(title_id, "NPHA", 4) == 0)
        return 4; /* Asia */
    if (strncmp(title_id, "BLKS", 4) == 0 ||
        strncmp(title_id, "BCKS", 4) == 0 ||
        strncmp(title_id, "NPKS", 4) == 0)
        return 5; /* Korea */
    return 0;
}

const char *ps3scale_community_region_label(const char *title_id)
{
    switch (title_region(title_id)) {
    case 1: return "US";
    case 2: return "EU";
    case 3: return "JAPAN";
    case 4: return "ASIA";
    case 5: return "KOREA";
    default: return "OTHER";
    }
}

int ps3scale_community_file_matches_exact_game(
    const ps3scale_community_file_t *file,
    const ps3scale_game_t *game)
{
    int file_region, game_region;

    if (!file || !game || !is_ps3_game(game->kind) ||
        !file->app_ver[0] || !game->app_ver[0] ||
        strcmp(file->app_ver, game->app_ver) != 0)
        return 0;

    if (file->title_id[0] && game->title_id[0] &&
        strcmp(file->title_id, game->title_id) == 0)
        return 1;

    if (!titles_match(game->title, file->title))
        return 0;

    file_region = title_region(file->title_id);
    game_region = title_region(game->title_id);
    return file_region != 0 && file_region == game_region;
}

const char *ps3scale_community_category_label(const char *category)
{
    if (!category)
        return "COMMUNITY FILE";
    if (strcmp(category, "GRAPHICS_FPS") == 0)
        return "GRAPHICS / FPS";
    if (strcmp(category, "GAMEPLAY_CHEAT") == 0)
        return "GAMEPLAY CHEAT";
    if (strcmp(category, "GAMEPLAY_CODE") == 0)
        return "GAMEPLAY CODE";
    if (strcmp(category, "COMMUNITY_CODE") == 0)
        return "COMMUNITY CODE";
    return "COMMUNITY FILE";
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
