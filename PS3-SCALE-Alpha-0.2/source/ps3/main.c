#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tiny3d.h>
#include <libfont.h>
#include <io/pad.h>
#include <sysutil/sysutil.h>

#include "ps3scale/game.h"
#include "ps3scale/scanner.h"
#include "font8x12.h"

enum {
    LIBRARY_PS3 = 0,
    LIBRARY_PS2 = 1
};

static ps3scale_game_list_t g_games;
static int g_selected = 0; /* Index inside the currently filtered library. */
static int g_library = LIBRARY_PS3;
static int g_exit_requested = 0;
static u32 *g_font_texture = NULL;

static void app_sysutil_callback(u64 status, u64 param, void *userdata)
{
    (void)param;
    (void)userdata;
    if (status == SYSUTIL_EXIT_GAME)
        g_exit_requested = 1;
}

static void draw_text(float x, float y, u32 color, char *text)
{
    SetFontColor(color, 0x00000000);
    DrawString(x, y, text);
}

static int game_is_ps3(const ps3scale_game_t *game)
{
    if (!game) return 0;
    return game->kind == PS3SCALE_GAME_PS3_FOLDER ||
           game->kind == PS3SCALE_GAME_PS3_HDD ||
           game->kind == PS3SCALE_GAME_PS3_ISO;
}

static int game_is_ps2(const ps3scale_game_t *game)
{
    if (!game) return 0;
    return game->kind == PS3SCALE_GAME_PS2_CLASSIC ||
           game->kind == PS3SCALE_GAME_PS2_HDD ||
           game->kind == PS3SCALE_GAME_PS2_ISO;
}

static int game_matches_library(const ps3scale_game_t *game)
{
    return g_library == LIBRARY_PS3 ? game_is_ps3(game) : game_is_ps2(game);
}

static size_t library_count(int library)
{
    size_t i;
    size_t count = 0;

    for (i = 0; i < g_games.count; i++) {
        if (library == LIBRARY_PS3 ? game_is_ps3(&g_games.items[i])
                                   : game_is_ps2(&g_games.items[i]))
            count++;
    }
    return count;
}

static const ps3scale_game_t *library_game_at(size_t visible_index)
{
    size_t i;
    size_t current = 0;

    for (i = 0; i < g_games.count; i++) {
        if (!game_matches_library(&g_games.items[i]))
            continue;
        if (current == visible_index)
            return &g_games.items[i];
        current++;
    }
    return NULL;
}

static void clamp_selection(void)
{
    size_t count = library_count(g_library);
    if (count == 0)
        g_selected = 0;
    else if (g_selected >= (int)count)
        g_selected = (int)count - 1;
}

static void draw_ui(void)
{
    char line[256];
    size_t i;
    size_t count;
    size_t start;
    size_t ps3_count;
    size_t ps2_count;
    const ps3scale_game_t *selected;

    count = library_count(g_library);
    ps3_count = library_count(LIBRARY_PS3);
    ps2_count = library_count(LIBRARY_PS2);
    clamp_selection();

    tiny3d_Clear(0xff070a0d, TINY3D_CLEAR_ALL);
    tiny3d_Project2D();

    /* Header and library tabs */
    draw_text(28, 22, 0xcaff39ff, "PS3 SCALE");
    draw_text(180, 25, 0x9aa6adff, "GAME LIBRARY  |  ALPHA 0.2");
    draw_text(28, 58, 0xffffffff, "LIBRARY");

    draw_text(28, 86,
              g_library == LIBRARY_PS3 ? 0xcaff39ff : 0x9aa6adff,
              "L1  PS3 GAMES");
    draw_text(190, 86,
              g_library == LIBRARY_PS2 ? 0xcaff39ff : 0x9aa6adff,
              "R1  PS2 GAMES");

    snprintf(line, sizeof(line), "PS3: %u   |   PS2: %u   |   SCAN: READ-ONLY",
             (unsigned)ps3_count, (unsigned)ps2_count);
    draw_text(28, 116, 0x9aa6adff, line);

    /* Filtered game list */
    draw_text(28, 154, 0xffffffff,
              g_library == LIBRARY_PS3 ? "PS3 GAME LIBRARY" : "PS2 GAME LIBRARY");
    start = (g_selected > 7) ? (size_t)(g_selected - 7) : 0;

    for (i = 0; i < 8 && start + i < count; i++) {
        const ps3scale_game_t *game = library_game_at(start + i);
        if (!game) continue;

        snprintf(line, sizeof(line), "%c %-12s  %.54s",
                 ((int)(start + i) == g_selected) ? '>' : ' ',
                 ps3scale_game_kind_name(game->kind), game->title);
        draw_text(34, 184 + (float)i * 30.0f,
                  ((int)(start + i) == g_selected) ? 0xcaff39ff : 0xdce3e8ff,
                  line);
    }

    /* Selected title details */
    draw_text(540, 154, 0xffffffff, "SELECTED TITLE");
    selected = count ? library_game_at((size_t)g_selected) : NULL;

    if (selected) {
        snprintf(line, sizeof(line), "TITLE: %.78s", selected->title);
        draw_text(540, 188, 0xdce3e8ff, line);

        if (g_library == LIBRARY_PS3) {
            snprintf(line, sizeof(line), "TITLE ID: %.12s",
                     selected->title_id[0] ? selected->title_id : "NOT READ");
            draw_text(540, 218, 0xdce3e8ff, line);
            snprintf(line, sizeof(line), "CATEGORY: %.6s",
                     selected->category[0] ? selected->category : "ISO");
            draw_text(540, 248, 0xdce3e8ff, line);
        } else {
            snprintf(line, sizeof(line), "PS2 SERIAL: %.12s",
                     selected->disc_id[0] ? selected->disc_id :
                     (selected->title_id[0] ? selected->title_id : "NOT READ"));
            draw_text(540, 218, 0xdce3e8ff, line);
            snprintf(line, sizeof(line), "SOURCE: %.24s",
                     ps3scale_game_kind_name(selected->kind));
            draw_text(540, 248, 0xdce3e8ff, line);
        }

        snprintf(line, sizeof(line), "PATCH STATUS: NONE / READ-ONLY");
        draw_text(540, 294, 0xcaff39ff, line);
        draw_text(540, 324, 0x9aa6adff, "Original game files have not been changed.");
        snprintf(line, sizeof(line), "PATH: %.70s", selected->path);
        draw_text(540, 366, 0x6f7b83ff, line);
    } else {
        draw_text(540, 188, 0x9aa6adff,
                  g_library == LIBRARY_PS3 ?
                  "No PS3 games found yet." : "No PS2 games found yet.");
        if (g_library == LIBRARY_PS3)
            draw_text(540, 218, 0x9aa6adff,
                      "Check GAMES, GAMEZ, game and PS3ISO.");
        else
            draw_text(540, 218, 0x9aa6adff,
                      "Check PS2ISO and installed PS2 Classics.");
    }

    if (g_games.truncated)
        draw_text(28, 408, 0xffcc66ff, "WARNING: LIBRARY LIMIT REACHED (256 TITLES)");

    /* Status and controls */
    draw_text(28, 448, 0x9aa6adff,
              "L1/R1: SWITCH LIBRARY   UP/DOWN: SELECT   SQUARE: RESCAN   CIRCLE: EXIT");
    draw_text(28, 478, 0x6f7b83ff,
              "GAME PATCHING IS DISABLED UNTIL A VERIFIED PER-TITLE BACKEND EXISTS.");
    tiny3d_Flip();
}

static void scan_library(void)
{
    (void)ps3scale_scan_all(&g_games);
    g_selected = 0;
    clamp_selection();
}

int main(int argc, char *argv[])
{
    padInfo pad_info;
    padData pad_data;
    u16 old_up = 0, old_down = 0, old_square = 0;
    u16 old_cross = 0, old_circle = 0, old_l1 = 0, old_r1 = 0;
    int pad_ready = 0;

    (void)argc;
    (void)argv;

    ps3scale_game_list_init(&g_games);

    tiny3d_Init(2 * 1024 * 1024);
    g_font_texture = tiny3d_AllocTexture(2 * 1024 * 1024);
    if (!g_font_texture) return 1;

    ResetFont();
    AddFontFromBitmapArray((u8 *)ps3scale_font8x12,
                           (u8 *)g_font_texture,
                           32, 126, 8, 12, 1, BIT7_FIRST_PIXEL);
    SetCurrentFont(0);
    SetFontSize(14, 22);

    if (ioPadInit(7) == 0) pad_ready = 1;
    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, app_sysutil_callback, NULL);

    scan_library();

    while (!g_exit_requested) {
        u16 up = 0, down = 0, square = 0, cross = 0, circle = 0;
        u16 l1 = 0, r1 = 0;

        sysUtilCheckCallback();

        if (pad_ready && ioPadGetInfo(&pad_info) == 0) {
            int p;
            for (p = 0; p < MAX_PADS; p++) {
                if (!pad_info.status[p]) continue;
                if (ioPadGetData(p, &pad_data) == 0 && pad_data.len >= 8) {
                    up = pad_data.BTN_UP;
                    down = pad_data.BTN_DOWN;
                    square = pad_data.BTN_SQUARE;
                    cross = pad_data.BTN_CROSS;
                    circle = pad_data.BTN_CIRCLE;
                    l1 = pad_data.BTN_L1;
                    r1 = pad_data.BTN_R1;
                    break;
                }
            }
        }

        if (l1 && !old_l1) {
            g_library = LIBRARY_PS3;
            g_selected = 0;
        }
        if (r1 && !old_r1) {
            g_library = LIBRARY_PS2;
            g_selected = 0;
        }

        if (up && !old_up && library_count(g_library)) {
            int count = (int)library_count(g_library);
            g_selected = (g_selected > 0) ? g_selected - 1 : count - 1;
        }
        if (down && !old_down && library_count(g_library)) {
            g_selected = (g_selected + 1) % (int)library_count(g_library);
        }
        if (square && !old_square)
            scan_library();
        if (cross && !old_cross) {
            /* Details are already shown in the right-hand panel. */
        }
        if (circle && !old_circle)
            g_exit_requested = 1;

        old_up = up;
        old_down = down;
        old_square = square;
        old_cross = cross;
        old_circle = circle;
        old_l1 = l1;
        old_r1 = r1;

        draw_ui();
    }

    sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
    if (pad_ready) ioPadEnd();
    return 0;
}
