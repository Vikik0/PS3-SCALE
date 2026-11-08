#ifndef PS3SCALE_CATALOG_H
#define PS3SCALE_CATALOG_H

#include <stddef.h>
#include "ps3scale/game.h"

#define PS3SCALE_CATALOG_MAX_PROFILES 128
#define PS3SCALE_PROFILE_TITLE_MAX   80
#define PS3SCALE_PROFILE_EFFECT_MAX  96
#define PS3SCALE_PROFILE_TRUST_MAX   24
#define PS3SCALE_PROFILE_PATCHES_MAX 256
#define PS3SCALE_PROFILE_NOTES_MAX   128
#define PS3SCALE_PROFILE_SOURCE_MAX  192
#define PS3SCALE_PROFILE_MAX_PATCHES 8

typedef struct ps3scale_profile {
    char title_id[10];
    char app_ver[6];
    char title[PS3SCALE_PROFILE_TITLE_MAX];
    char effect[PS3SCALE_PROFILE_EFFECT_MAX];
    char trust[PS3SCALE_PROFILE_TRUST_MAX];
    char patches[PS3SCALE_PROFILE_PATCHES_MAX];
    char notes[PS3SCALE_PROFILE_NOTES_MAX];
    char source_url[PS3SCALE_PROFILE_SOURCE_MAX];
    unsigned patch_count;
} ps3scale_profile_t;

typedef struct ps3scale_catalog {
    ps3scale_profile_t items[PS3SCALE_CATALOG_MAX_PROFILES];
    size_t count;
} ps3scale_catalog_t;

/* Returns the loaded profile count (>0), or -1 when the entire file is invalid. */
int ps3scale_catalog_load(const char *path, ps3scale_catalog_t *catalog);

/* Exact Title ID + APP_VER match only. Missing versions never match. */
const ps3scale_profile_t *ps3scale_catalog_match(
    const ps3scale_catalog_t *catalog,
    const char *title_id,
    const char *app_ver);

const ps3scale_profile_t *ps3scale_catalog_match_game(
    const ps3scale_catalog_t *catalog,
    const ps3scale_game_t *game);

size_t ps3scale_catalog_count_matches(
    const ps3scale_catalog_t *catalog,
    const ps3scale_game_list_t *games);

#endif
