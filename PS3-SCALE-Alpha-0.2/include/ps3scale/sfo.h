#ifndef PS3SCALE_SFO_H
#define PS3SCALE_SFO_H

#include <stddef.h>
#include <stdint.h>

#define PS3SCALE_TITLE_MAX       128
#define PS3SCALE_TITLE_ID_MAX    16
#define PS3SCALE_CATEGORY_MAX    8
#define PS3SCALE_APP_VER_MAX     16

typedef struct ps3scale_sfo_info {
    char title[PS3SCALE_TITLE_MAX];
    char title_id[PS3SCALE_TITLE_ID_MAX];
    char category[PS3SCALE_CATEGORY_MAX];
    char app_ver[PS3SCALE_APP_VER_MAX];
} ps3scale_sfo_info_t;

/* Returns 0 on success, negative on malformed/missing required data. */
int ps3scale_sfo_parse(const uint8_t *data, size_t size,
                       ps3scale_sfo_info_t *out);

int ps3scale_title_id_valid(const char *title_id);

#endif
