#ifndef PS3SCALE_COMMUNITY_H
#define PS3SCALE_COMMUNITY_H

#include <stddef.h>
#include "ps3scale/game.h"

#define PS3SCALE_COMMUNITY_MAX_FILES 8192
#define PS3SCALE_COMMUNITY_TITLE_MAX 128
#define PS3SCALE_COMMUNITY_KIND_MAX 32
#define PS3SCALE_COMMUNITY_CATEGORY_MAX 24
#define PS3SCALE_COMMUNITY_DESCRIPTION_MAX 128
#define PS3SCALE_COMMUNITY_REPO_MAX 80
#define PS3SCALE_COMMUNITY_BRANCH_MAX 24
#define PS3SCALE_COMMUNITY_PATH_MAX 384

typedef struct ps3scale_community_file {
    char title_id[PS3SCALE_ID_MAX];
    char app_ver[PS3SCALE_GAME_APP_VER_MAX];
    char title[PS3SCALE_COMMUNITY_TITLE_MAX];
    char source_kind[PS3SCALE_COMMUNITY_KIND_MAX];
    char category[PS3SCALE_COMMUNITY_CATEGORY_MAX];
    char description[PS3SCALE_COMMUNITY_DESCRIPTION_MAX];
    char repository[PS3SCALE_COMMUNITY_REPO_MAX];
    char branch[PS3SCALE_COMMUNITY_BRANCH_MAX];
    char path[PS3SCALE_COMMUNITY_PATH_MAX];
    unsigned long file_size;
} ps3scale_community_file_t;

typedef struct ps3scale_community_index {
    ps3scale_community_file_t *items;
    size_t count;
} ps3scale_community_index_t;

int ps3scale_community_index_load(
    const char *path,
    ps3scale_community_index_t *index);
void ps3scale_community_index_free(ps3scale_community_index_t *index);
int ps3scale_community_file_matches_game(
    const ps3scale_community_file_t *file,
    const ps3scale_game_t *game);
const char *ps3scale_community_source_label(const char *source_kind);
const char *ps3scale_community_category_label(const char *category);
const char *ps3scale_community_region_label(const char *title_id);
int ps3scale_community_file_matches_exact_game(
    const ps3scale_community_file_t *file,
    const ps3scale_game_t *game);

#endif
