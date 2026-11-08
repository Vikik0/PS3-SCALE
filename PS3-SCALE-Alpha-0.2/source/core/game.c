#include "ps3scale/game.h"

#include <string.h>

void ps3scale_game_list_init(ps3scale_game_list_t *list)
{
    if (list) memset(list, 0, sizeof(*list));
}

int ps3scale_game_list_add(ps3scale_game_list_t *list,
                           const ps3scale_game_t *game)
{
    size_t i;
    if (!list || !game) return -1;

    for (i = 0; i < list->count; i++) {
        const ps3scale_game_t *old = &list->items[i];
        if (strcmp(old->path, game->path) == 0)
            return 0;
    }

    if (list->count >= PS3SCALE_MAX_GAMES) {
        list->truncated = 1;
        return -2;
    }

    list->items[list->count++] = *game;
    return 1;
}

const char *ps3scale_game_kind_name(ps3scale_game_kind_t kind)
{
    switch (kind) {
        case PS3SCALE_GAME_PS3_FOLDER: return "PS3 FOLDER";
        case PS3SCALE_GAME_PS3_HDD: return "PS3 HDD";
        case PS3SCALE_GAME_PS3_ISO: return "PS3 ISO";
        case PS3SCALE_GAME_PS2_CLASSIC: return "PS2 CLASSIC";
        case PS3SCALE_GAME_PS2_HDD: return "PS2 HDD";
        case PS3SCALE_GAME_PS2_ISO: return "PS2 ISO";
        default: return "UNKNOWN";
    }
}
