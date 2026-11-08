#ifndef PS3SCALE_GAME_H
#define PS3SCALE_GAME_H

#include <stddef.h>

#define PS3SCALE_MAX_GAMES 256
#define PS3SCALE_PATH_MAX  512
#define PS3SCALE_NAME_MAX  128
#define PS3SCALE_ID_MAX    16
#define PS3SCALE_DISC_ID_MAX 16
#define PS3SCALE_CAT_MAX   8

typedef enum ps3scale_game_kind {
    PS3SCALE_GAME_PS3_FOLDER = 0,
    PS3SCALE_GAME_PS3_HDD,
    PS3SCALE_GAME_PS3_ISO,
    PS3SCALE_GAME_PS2_CLASSIC,
    PS3SCALE_GAME_PS2_HDD,
    PS3SCALE_GAME_PS2_ISO
} ps3scale_game_kind_t;

typedef struct ps3scale_game {
    ps3scale_game_kind_t kind;
    char title[PS3SCALE_NAME_MAX];
    char title_id[PS3SCALE_ID_MAX];
    char disc_id[PS3SCALE_DISC_ID_MAX]; /* PS2 serial, e.g. SLUS_203.12 */
    char category[PS3SCALE_CAT_MAX];
    char path[PS3SCALE_PATH_MAX];
    char cover_path[PS3SCALE_PATH_MAX]; /* Local ICON0.PNG when present. */
} ps3scale_game_t;

typedef struct ps3scale_game_list {
    ps3scale_game_t items[PS3SCALE_MAX_GAMES];
    size_t count;
    unsigned truncated;
} ps3scale_game_list_t;

void ps3scale_game_list_init(ps3scale_game_list_t *list);
int ps3scale_game_list_add(ps3scale_game_list_t *list,
                           const ps3scale_game_t *game);
const char *ps3scale_game_kind_name(ps3scale_game_kind_t kind);

#endif
