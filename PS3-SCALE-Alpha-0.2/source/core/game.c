#include "ps3scale/game.h"

#include <ctype.h>
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

static int region_prefix_matches(const char prefix[4], const char *candidate)
{
    return prefix[0] == candidate[0] && prefix[1] == candidate[1] &&
           prefix[2] == candidate[2] && prefix[3] == candidate[3];
}

const char *ps3scale_game_region_name(const char *id)
{
    char prefix[4];
    size_t count = 0;

    if (!id)
        return "UNKNOWN";
    while (*id && count < sizeof(prefix)) {
        unsigned char ch = (unsigned char)*id++;
        if (isalnum(ch))
            prefix[count++] = (char)toupper(ch);
    }
    if (count != sizeof(prefix))
        return "UNKNOWN";

    if (region_prefix_matches(prefix, "SLUS") ||
        region_prefix_matches(prefix, "SCUS") ||
        region_prefix_matches(prefix, "BLUS") ||
        region_prefix_matches(prefix, "BCUS") ||
        region_prefix_matches(prefix, "NPUB") ||
        region_prefix_matches(prefix, "NPUS") ||
        region_prefix_matches(prefix, "NPUD") ||
        region_prefix_matches(prefix, "PS2U"))
        return "NORTH AMERICA";
    if (region_prefix_matches(prefix, "SLES") ||
        region_prefix_matches(prefix, "SCES") ||
        region_prefix_matches(prefix, "BLES") ||
        region_prefix_matches(prefix, "BCES") ||
        region_prefix_matches(prefix, "NPEB") ||
        region_prefix_matches(prefix, "NPEA") ||
        region_prefix_matches(prefix, "NPED") ||
        region_prefix_matches(prefix, "PS2E"))
        return "EUROPE";
    if (region_prefix_matches(prefix, "SLPM") ||
        region_prefix_matches(prefix, "SLPS") ||
        region_prefix_matches(prefix, "SCPS") ||
        region_prefix_matches(prefix, "SCAJ") ||
        region_prefix_matches(prefix, "BLJS") ||
        region_prefix_matches(prefix, "BLJM") ||
        region_prefix_matches(prefix, "BCJS") ||
        region_prefix_matches(prefix, "NPJB") ||
        region_prefix_matches(prefix, "NPJD") ||
        region_prefix_matches(prefix, "NPJJ") ||
        region_prefix_matches(prefix, "NPJA") ||
        region_prefix_matches(prefix, "PS2J"))
        return "JAPAN";
    if (region_prefix_matches(prefix, "SLKA") ||
        region_prefix_matches(prefix, "SCKA") ||
        region_prefix_matches(prefix, "BLKS") ||
        region_prefix_matches(prefix, "BCKS") ||
        region_prefix_matches(prefix, "NPKB") ||
        region_prefix_matches(prefix, "NPKS"))
        return "KOREA";
    if (region_prefix_matches(prefix, "BLAS") ||
        region_prefix_matches(prefix, "BCAS") ||
        region_prefix_matches(prefix, "NPHB") ||
        region_prefix_matches(prefix, "NPHA"))
        return "ASIA";
    return "UNKNOWN";
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
