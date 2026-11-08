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

static ps3scale_game_list_t g_games;
static int g_selected = 0;
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

static void draw_ui(void)
{
    char line[256];
    size_t i;
    size_t start = 0;

    tiny3d_Clear(0xff070a0d, TINY3D_CLEAR_ALL);
    tiny3d_Project2D();

    /* Header */
    draw_text(28, 22, 0xcaff39ff, "PS3 SCALE");
    draw_text(180, 25, 0x9aa6adff, "GRAPHICS ENHANCEMENT MANAGER  |  ALPHA 0.1");
    draw_text(28, 58, 0xffffffff, "LIBRARY");
    snprintf(line, sizeof(line), "PLATFORM: PS3 GAMEOS  |  SCAN MODE: READ-ONLY");
    draw_text(28, 82, 0x9aa6adff, line);
    snprintf(line, sizeof(line), "GAMES FOUND: %u", (unsigned)g_games.count);
    draw_text(28, 108, 0xcaff39ff, line);

    /* List */
    draw_text(28, 150, 0xffffffff, "GAME LIBRARY");
    start = (g_selected > 7) ? (size_t)(g_selected - 7) : 0;
    for (i = 0; i < 8 && start + i < g_games.count; i++) {
        const ps3scale_game_t *game = &g_games.items[start + i];
        snprintf(line, sizeof(line), "%c %-12s  %s",
                 ((int)(start + i) == g_selected) ? '>' : ' ',
                 ps3scale_game_kind_name(game->kind), game->title);
        draw_text(34, 180 + (float)i * 30.0f,
                  ((int)(start + i) == g_selected) ? 0xcaff39ff : 0xdce3e8ff,
                  line);
    }

    /* Details */
    draw_text(540, 150, 0xffffffff, "SELECTED TITLE");
    if (g_games.count > 0 && g_selected >= 0 &&
        (size_t)g_selected < g_games.count) {
        const ps3scale_game_t *game = &g_games.items[g_selected];
        snprintf(line, sizeof(line), "TITLE: %.90s", game->title);
        draw_text(540, 184, 0xdce3e8ff, line);
        snprintf(line, sizeof(line), "TITLE ID: %.12s",
                 game->title_id[0] ? game->title_id : "NOT READ");
        draw_text(540, 214, 0xdce3e8ff, line);
        snprintf(line, sizeof(line), "CATEGORY: %.6s",
                 game->category[0] ? game->category : "ISO");
        draw_text(540, 244, 0xdce3e8ff, line);
        if (game->disc_id[0]) {
            snprintf(line, sizeof(line), "PS2 DISC ID: %.12s", game->disc_id);
            draw_text(540, 274, 0xdce3e8ff, line);
        }
        draw_text(540, 312, 0xcaff39ff, "PROFILE: SAFE / READ-ONLY");
        draw_text(540, 342, 0x9aa6adff, "No game files have been changed.");
    } else {
        draw_text(540, 184, 0x9aa6adff, "No supported games found yet.");
        draw_text(540, 214, 0x9aa6adff, "Check GAMES, game, PS3ISO and PS2ISO.");
    }

    /* Status and controls */
    draw_text(28, 432, 0x9aa6adff,
              "X: DETAILS    SQUARE: RESCAN    UP/DOWN: SELECT    CIRCLE: EXIT");
    draw_text(28, 466, 0x6f7b83ff,
              "GRAPHICS PATCHING IS DISABLED IN THIS SCANNER BUILD.");
    tiny3d_Flip();
}

static void scan_library(void)
{
    (void)ps3scale_scan_all(&g_games);
    if (g_selected >= (int)g_games.count)
        g_selected = g_games.count ? (int)g_games.count - 1 : 0;
}

int main(int argc, char *argv[])
{
    padInfo pad_info;
    padData pad_data;
    u16 old_up = 0, old_down = 0, old_square = 0, old_cross = 0, old_circle = 0;
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
                    break;
                }
            }
        }

        if (up && !old_up && g_games.count)
            g_selected = (g_selected > 0) ? g_selected - 1 : (int)g_games.count - 1;
        if (down && !old_down && g_games.count)
            g_selected = (g_selected + 1) % (int)g_games.count;
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

        draw_ui();
    }

    sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
    if (pad_ready) ioPadEnd();
    return 0;
}
