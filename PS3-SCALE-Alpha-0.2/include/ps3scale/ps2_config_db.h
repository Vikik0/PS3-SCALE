#ifndef PS3SCALE_PS2_CONFIG_DB_H
#define PS3SCALE_PS2_CONFIG_DB_H

#include <stddef.h>

#define PS3SCALE_PS2_CONFIG_MAX_ENTRIES 5000u
#define PS3SCALE_PS2_CONFIG_TYPE_MAX 8
#define PS3SCALE_PS2_CONFIG_ID_MAX 16
#define PS3SCALE_PS2_CONFIG_TITLE_MAX 128

typedef struct ps3scale_ps2_config_entry {
    char type[PS3SCALE_PS2_CONFIG_TYPE_MAX];
    char title_id[PS3SCALE_PS2_CONFIG_ID_MAX];
    char title[PS3SCALE_PS2_CONFIG_TITLE_MAX];
} ps3scale_ps2_config_entry_t;

typedef struct ps3scale_ps2_config_db {
    ps3scale_ps2_config_entry_t *items;
    size_t count;
} ps3scale_ps2_config_db_t;

int ps3scale_ps2_config_db_load(const char *path,
                                ps3scale_ps2_config_db_t *db);
void ps3scale_ps2_config_db_free(ps3scale_ps2_config_db_t *db);
int ps3scale_ps2_config_normalize_id(const char *source,
                                     char *output, size_t output_size);
const ps3scale_ps2_config_entry_t *ps3scale_ps2_config_db_find(
    const ps3scale_ps2_config_db_t *db,
    const char *title_id,
    const char *preferred_type);
int ps3scale_ps2_config_build_url(const ps3scale_ps2_config_entry_t *entry,
                                  char *url, size_t url_size);

#endif
