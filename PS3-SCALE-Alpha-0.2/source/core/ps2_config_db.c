#include "ps3scale/ps2_config_db.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PS2_CONFIG_LINE_MAX 256

static int valid_type(const char *type)
{
    return type &&
        (strcmp(type, "CUSTOM") == 0 ||
         strcmp(type, "NET") == 0 ||
         strcmp(type, "GX") == 0 ||
         strcmp(type, "SOFT") == 0 ||
         strcmp(type, "ENC") == 0);
}

static int valid_id_chars(const char *value)
{
    size_t i;
    if (!value || !value[0] || strlen(value) >= PS3SCALE_PS2_CONFIG_ID_MAX)
        return 0;
    for (i = 0; value[i]; i++) {
        unsigned char ch = (unsigned char)value[i];
        if (!(isalnum(ch) || ch == '_' || ch == '.' || ch == '-'))
            return 0;
    }
    return 1;
}

static int is_serial_prefix(const char *value)
{
    static const char *prefixes[] = {
        "SLUS", "SLES", "SLPM", "SLPS", "SCUS", "SCES",
        "SCPS", "SLKA", "SCAJ", "SLPA", "SLAJ", "PAPX"
    };
    size_t i;
    for (i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++)
        if (strncmp(value, prefixes[i], 4) == 0)
            return 1;
    return 0;
}

int ps3scale_ps2_config_normalize_id(const char *source,
                                     char *output, size_t output_size)
{
    char compact[16];
    size_t i, count = 0;
    int had_separator = 0;

    if (!source || !output || output_size < 10)
        return -1;
    output[0] = '\0';

    for (i = 0; source[i]; i++) {
        unsigned char ch = (unsigned char)source[i];
        if (isalnum(ch)) {
            if (count + 1 >= sizeof(compact))
                return -1;
            compact[count++] = (char)toupper(ch);
        } else if (ch == '_' || ch == '-' || ch == '.' || ch == ' ') {
            had_separator = 1;
        } else {
            return -1;
        }
    }
    compact[count] = '\0';

    if (count == 9 &&
        isalpha((unsigned char)compact[0]) &&
        isalpha((unsigned char)compact[1]) &&
        isalpha((unsigned char)compact[2]) &&
        isalpha((unsigned char)compact[3]) &&
        isdigit((unsigned char)compact[4]) &&
        isdigit((unsigned char)compact[5]) &&
        isdigit((unsigned char)compact[6]) &&
        isdigit((unsigned char)compact[7]) &&
        isdigit((unsigned char)compact[8]) &&
        (had_separator || is_serial_prefix(compact))) {
        int written = snprintf(output, output_size, "%.4s_%.3s.%.2s",
                               compact, compact + 4, compact + 7);
        return (written < 0 || (size_t)written >= output_size) ? -1 : 0;
    }

    if (count == 0 || count >= output_size)
        return -1;
    memcpy(output, compact, count + 1);
    return 0;
}

static int parse_line(char *line, ps3scale_ps2_config_entry_t *entry)
{
    char *type, *id, *title, *tab;
    size_t title_length;

    if (!line || !entry || line[0] == '#' || !line[0])
        return -1;
    type = line;
    tab = strchr(type, '\t');
    if (!tab) return -1;
    *tab++ = '\0';
    id = tab;
    tab = strchr(id, '\t');
    if (!tab) return -1;
    *tab++ = '\0';
    title = tab;

    if (!valid_type(type) || !valid_id_chars(id))
        return -1;
    title_length = strlen(title);
    while (title_length &&
           (title[title_length - 1] == '\r' ||
            title[title_length - 1] == '\n'))
        title[--title_length] = '\0';
    if (!title_length)
        return -1;

    memset(entry, 0, sizeof(*entry));
    snprintf(entry->type, sizeof(entry->type), "%s", type);
    snprintf(entry->title_id, sizeof(entry->title_id), "%s", id);
    snprintf(entry->title, sizeof(entry->title), "%s", title);
    return 0;
}

int ps3scale_ps2_config_db_load(const char *path,
                                ps3scale_ps2_config_db_t *db)
{
    FILE *fp;
    char line[PS2_CONFIG_LINE_MAX];
    ps3scale_ps2_config_entry_t *items;
    size_t count = 0;

    if (!path || !db)
        return -1;
    fp = fopen(path, "rb");
    if (!fp)
        return -1;

    items = (ps3scale_ps2_config_entry_t *)calloc(
        PS3SCALE_PS2_CONFIG_MAX_ENTRIES, sizeof(*items));
    if (!items) {
        fclose(fp);
        return -1;
    }

    while (fgets(line, sizeof(line), fp)) {
        size_t length = strlen(line);
        ps3scale_ps2_config_entry_t entry;
        if (length && line[length - 1] != '\n' && !feof(fp)) {
            fclose(fp);
            free(items);
            return -1;
        }
        while (length && (line[length - 1] == '\n' ||
                          line[length - 1] == '\r'))
            line[--length] = '\0';
        if (!length || line[0] == '#')
            continue;
        if (count >= PS3SCALE_PS2_CONFIG_MAX_ENTRIES ||
            parse_line(line, &entry) != 0)
            continue;
        items[count++] = entry;
    }

    if (ferror(fp)) {
        fclose(fp);
        free(items);
        return -1;
    }
    fclose(fp);
    if (!count) {
        free(items);
        return -1;
    }

    {
        ps3scale_ps2_config_entry_t *smaller =
            (ps3scale_ps2_config_entry_t *)realloc(
                items, count * sizeof(*items));
        if (smaller)
            items = smaller;
    }
    free(db->items);
    db->items = items;
    db->count = count;
    return (int)count;
}

void ps3scale_ps2_config_db_free(ps3scale_ps2_config_db_t *db)
{
    if (!db)
        return;
    free(db->items);
    db->items = NULL;
    db->count = 0;
}

static int type_score(const char *type, const char *preferred_type)
{
    if (!type)
        return 0;
    if (preferred_type && strcmp(preferred_type, "ENC") == 0)
        return strcmp(type, "ENC") == 0 ? 100 : 0;
    if (strcmp(type, "CUSTOM") == 0) return 100;
    if (preferred_type && strcmp(type, preferred_type) == 0) return 90;
    if (strcmp(type, "NET") == 0) return 80;
    if (strcmp(type, "GX") == 0) return 70;
    if (strcmp(type, "SOFT") == 0) return 60;
    return 0;
}

const ps3scale_ps2_config_entry_t *ps3scale_ps2_config_db_find(
    const ps3scale_ps2_config_db_t *db,
    const char *title_id,
    const char *preferred_type)
{
    char wanted[PS3SCALE_PS2_CONFIG_ID_MAX];
    const ps3scale_ps2_config_entry_t *best = NULL;
    int best_score = 0;
    size_t i;

    if (!db || !db->items || !title_id ||
        ps3scale_ps2_config_normalize_id(title_id, wanted,
                                         sizeof(wanted)) != 0)
        return NULL;

    for (i = 0; i < db->count; i++) {
        char candidate[PS3SCALE_PS2_CONFIG_ID_MAX];
        int score;
        if (ps3scale_ps2_config_normalize_id(db->items[i].title_id,
                                             candidate,
                                             sizeof(candidate)) != 0 ||
            strcmp(candidate, wanted) != 0)
            continue;
        score = type_score(db->items[i].type, preferred_type);
        if (score > best_score) {
            best = &db->items[i];
            best_score = score;
        }
    }
    return best;
}

int ps3scale_ps2_config_build_url(const ps3scale_ps2_config_entry_t *entry,
                                  char *url, size_t url_size)
{
    const char *extension;
    char normalized[PS3SCALE_PS2_CONFIG_ID_MAX];
    int written;

    if (!entry || !url || url_size == 0 ||
        ps3scale_ps2_config_normalize_id(entry->title_id, normalized,
                                         sizeof(normalized)) != 0 ||
        strcmp(normalized, entry->title_id) != 0)
        return -1;

    if (strcmp(entry->type, "ENC") == 0)
        extension = ".ENC";
    else if (strcmp(entry->type, "CUSTOM") == 0 ||
             strcmp(entry->type, "NET") == 0 ||
             strcmp(entry->type, "GX") == 0 ||
             strcmp(entry->type, "SOFT") == 0)
        extension = ".CONFIG";
    else
        return -1;

    written = snprintf(url, url_size,
        "https://raw.githubusercontent.com/aldostools/webMAN-MOD/master/"
        "_Projects_/updater/PS2CONFIG/USRDIR/CONFIG/%s/%s%s",
        entry->type, entry->title_id, extension);
    return (written < 0 || (size_t)written >= url_size) ? -1 : 0;
}
