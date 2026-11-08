#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <tiny3d.h>
#include <libfont.h>
#include <io/pad.h>
#include <sysutil/sysutil.h>
#include <sysutil/osk.h>
#include <sys/memory.h>
#include <ya2d/ya2d.h>

#include "ps3scale/game.h"
#include "ps3scale/scanner.h"
#include "ps3scale/ps2_config.h"
#include "font8x12.h"

enum {
    LIBRARY_PS3 = 0,
    LIBRARY_PS2 = 1
};

typedef enum app_screen {
    SCREEN_HOME = 0,
    SCREEN_SCAN,
    SCREEN_DETAILS,
    SCREEN_SETTINGS,
    SCREEN_ABOUT
} app_screen_t;

static ps3scale_game_list_t g_games;
static int g_selected = 0;
static int g_library = LIBRARY_PS3;
static int g_config_preview = 0;
static int g_exit_requested = 0;
static int g_home_selected = 0;
static app_screen_t g_screen = SCREEN_HOME;
static char g_search[64] = "";
static u32 *g_font_texture = NULL;
static ya2d_Texture *g_cover_texture = NULL;
static char g_cover_path[PS3SCALE_PATH_MAX] = "";

/* PS3 system on-screen keyboard state used by the Scan Games search field. */
static int g_osk_active = 0;
static sys_mem_container_t g_osk_container;
static u16 g_osk_title[64];
static u16 g_osk_start[64];
static u16 g_osk_text[64];
static oskInputFieldInfo g_osk_input;
static oskParam g_osk_params;
static oskCallbackReturnParam g_osk_result;

static void ascii_to_utf16(const char *src, u16 *dst, size_t capacity)
{
    size_t i = 0;
    if (!dst || capacity == 0) return;
    while (src && src[i] && i + 1 < capacity) {
        dst[i] = (u8)src[i];
        i++;
    }
    dst[i] = 0;
}

static void utf16_to_ascii(const u16 *src, char *dst, size_t capacity)
{
    size_t i = 0;
    if (!dst || capacity == 0) return;
    while (src && src[i] && i + 1 < capacity) {
        dst[i] = (src[i] < 128) ? (char)src[i] : '?';
        i++;
    }
    dst[i] = '\0';
}

static void app_sysutil_callback(u64 status, u64 param, void *userdata)
{
    (void)param;
    (void)userdata;

    if (status == SYSUTIL_EXIT_GAME) {
        g_exit_requested = 1;
    } else if (status == SYSUTIL_OSK_DONE) {
        (void)oskUnloadAsync(&g_osk_result);
    } else if (status == SYSUTIL_OSK_INPUT_CANCELED) {
        (void)oskAbort();
    } else if (status == SYSUTIL_OSK_UNLOADED) {
        g_osk_active = 0;
        if (g_osk_result.res == OSK_OK)
            utf16_to_ascii(g_osk_result.str, g_search, sizeof(g_search));
        (void)sysMemContainerDestroy(g_osk_container);
        g_selected = 0;
        g_config_preview = 0;
    }
}

static int open_search_keyboard(void)
{
    int rc;

    if (g_osk_active) return 0;

    memset(g_osk_title, 0, sizeof(g_osk_title));
    memset(g_osk_start, 0, sizeof(g_osk_start));
    memset(g_osk_text, 0, sizeof(g_osk_text));
    ascii_to_utf16("Search games by title:", g_osk_title,
                   sizeof(g_osk_title) / sizeof(g_osk_title[0]));
    ascii_to_utf16(g_search, g_osk_start,
                   sizeof(g_osk_start) / sizeof(g_osk_start[0]));

    g_osk_input.message = g_osk_title;
    g_osk_input.startText = g_osk_start;
    g_osk_input.maxLength = (s32)(sizeof(g_osk_text) / sizeof(g_osk_text[0])) - 1;

    g_osk_params.allowedPanels = OSK_PANEL_TYPE_DEFAULT;
    g_osk_params.firstViewPanel = OSK_PANEL_TYPE_DEFAULT;
    g_osk_params.controlPoint.x = 0.0f;
    g_osk_params.controlPoint.y = 0.0f;
    g_osk_params.prohibitFlags = OSK_PROHIBIT_RETURN;

    g_osk_result.res = OSK_OK;
    g_osk_result.len = g_osk_input.maxLength;
    g_osk_result.str = g_osk_text;

    rc = sysMemContainerCreate(&g_osk_container, 4 * 1024 * 1024);
    if (rc != 0) return rc;

    (void)oskSetInitialInputDevice(OSK_DEVICE_PAD);
    (void)oskSetKeyLayoutOption(OSK_FULLKEY_PANEL);
    (void)oskSetLayoutMode(OSK_LAYOUTMODE_HORIZONTAL_ALIGN_CENTER |
                           OSK_LAYOUTMODE_VERTICAL_ALIGN_CENTER);

    rc = oskLoadAsync(g_osk_container, &g_osk_params, &g_osk_input);
    if (rc != 0) {
        (void)sysMemContainerDestroy(g_osk_container);
        return rc;
    }

    g_osk_active = 1;
    return 0;
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

static int text_contains_ci(const char *text, const char *query)
{
    size_t i, j;
    if (!query || !query[0]) return 1;
    if (!text) return 0;

    for (i = 0; text[i]; i++) {
        for (j = 0; query[j]; j++) {
            if (!text[i + j]) break;
            if (tolower((unsigned char)text[i + j]) !=
                tolower((unsigned char)query[j]))
                break;
        }
        if (!query[j]) return 1;
    }
    return 0;
}

static int game_matches_library(const ps3scale_game_t *game)
{
    int platform_match =
        (g_library == LIBRARY_PS3) ? game_is_ps3(game) : game_is_ps2(game);

    return platform_match && text_contains_ci(game->title, g_search);
}

static size_t library_count(int library)
{
    size_t i, count = 0;
    for (i = 0; i < g_games.count; i++) {
        int platform_match =
            (library == LIBRARY_PS3) ? game_is_ps3(&g_games.items[i])
                                     : game_is_ps2(&g_games.items[i]);
        if (platform_match && text_contains_ci(g_games.items[i].title, g_search))
            count++;
    }
    return count;
}

static const ps3scale_game_t *library_game_at(size_t visible_index)
{
    size_t i, current = 0;
    for (i = 0; i < g_games.count; i++) {
        if (!game_matches_library(&g_games.items[i])) continue;
        if (current == visible_index) return &g_games.items[i];
        current++;
    }
    return NULL;
}

static const ps3scale_game_t *selected_game(void)
{
    size_t count = library_count(g_library);
    if (count == 0) return NULL;
    if (g_selected < 0 || (size_t)g_selected >= count) return NULL;
    return library_game_at((size_t)g_selected);
}

static void clamp_selection(void)
{
    size_t count = library_count(g_library);
    if (count == 0) g_selected = 0;
    else if (g_selected >= (int)count) g_selected = (int)count - 1;
}

static void format_hex(char *out, size_t capacity,
                       const uint8_t *data, size_t size)
{
    size_t i, used = 0;
    if (!out || capacity == 0) return;
    out[0] = '\0';

    for (i = 0; i < size && used < capacity; i++) {
        int n = snprintf(out + used, capacity - used,
                         i + 1 == size ? "%02X" : "%02X ",
                         (unsigned)data[i]);
        if (n < 0 || (size_t)n >= capacity - used) {
            out[capacity - 1] = '\0';
            return;
        }
        used += (size_t)n;
    }
}


static void update_selected_cover(void)
{
    const ps3scale_game_t *game = selected_game();
    const char *path = (game && game->cover_path[0]) ? game->cover_path : "";

    if (strcmp(path, g_cover_path) == 0)
        return;

    if (g_cover_texture) {
        ya2d_freeTexture(g_cover_texture);
        g_cover_texture = NULL;
    }
    g_cover_path[0] = '\0';

    if (path[0]) {
        g_cover_texture = ya2d_loadPNGfromFile(path);
        if (g_cover_texture)
            strncpy(g_cover_path, path, sizeof(g_cover_path) - 1);
    }
}

static void draw_home(void)
{
    static char *items[] = {
        "[G]  SCAN GAMES",
        "[S]  SETTINGS",
        "[i]  ABOUT PS3 SCALE"
    };
    int i;

    draw_text(34, 28, 0xcaff39ff, "PS3 SCALE");
    draw_text(34, 62, 0x9aa6adff, "GAME LIBRARY AND GRAPHICS MANAGER");
    draw_text(34, 122, 0xffffffff, "MAIN MENU");

    for (i = 0; i < 3; i++) {
        char line[96];
        snprintf(line, sizeof(line), "%c  %s",
                 (i == g_home_selected) ? '>' : ' ', items[i]);
        draw_text(62, 184 + (float)i * 58.0f,
                  (i == g_home_selected) ? 0xcaff39ff : 0xdce3e8ff,
                  line);
    }

    draw_text(34, 430, 0x9aa6adff, "UP/DOWN: SELECT   CROSS: OPEN   CIRCLE: EXIT");
    draw_text(34, 468, 0x6f7b83ff,
              "PS3 AND PS2 GAME SCANNING IS READ-ONLY.");
}

static void draw_scan(void)
{
    char line[256];
    size_t i, count, start;
    size_t ps3_count = library_count(LIBRARY_PS3);
    size_t ps2_count = library_count(LIBRARY_PS2);
    const ps3scale_game_t *selected = selected_game();

    count = library_count(g_library);
    clamp_selection();

    draw_text(28, 22, 0xcaff39ff, "PS3 SCALE");
    draw_text(180, 25, 0x9aa6adff, "SCAN GAMES");
    draw_text(28, 62, 0xffffffff, "PLATFORM");

    draw_text(28, 88, g_library == LIBRARY_PS3 ? 0xcaff39ff : 0x9aa6adff,
              "L1  PS3");
    draw_text(150, 88, g_library == LIBRARY_PS2 ? 0xcaff39ff : 0x9aa6adff,
              "R1  PS2");

    snprintf(line, sizeof(line), "SEARCH: %s", g_search[0] ? g_search : "ALL GAMES");
    draw_text(28, 120, 0x9aa6adff, line);
    snprintf(line, sizeof(line), "PS3: %u  |  PS2: %u  |  RESULTS: %u",
             (unsigned)ps3_count, (unsigned)ps2_count, (unsigned)count);
    draw_text(28, 148, 0x9aa6adff, line);

    draw_text(28, 184, 0xffffffff,
              g_library == LIBRARY_PS3 ? "PS3 GAME LIST" : "PS2 GAME LIST");
    start = (g_selected > 7) ? (size_t)(g_selected - 7) : 0;

    for (i = 0; i < 8 && start + i < count; i++) {
        const ps3scale_game_t *game = library_game_at(start + i);
        if (!game) continue;
        snprintf(line, sizeof(line), "%c %-12s %.44s",
                 ((int)(start + i) == g_selected) ? '>' : ' ',
                 ps3scale_game_kind_name(game->kind), game->title);
        draw_text(34, 214 + (float)i * 27.0f,
                  ((int)(start + i) == g_selected) ? 0xcaff39ff : 0xdce3e8ff,
                  line);
    }

    draw_text(570, 184, 0xffffffff, "SELECTED GAME");
    if (selected) {
        snprintf(line, sizeof(line), "TITLE: %.68s", selected->title);
        draw_text(570, 218, 0xdce3e8ff, line);
        snprintf(line, sizeof(line), "TYPE: %.28s",
                 ps3scale_game_kind_name(selected->kind));
        draw_text(570, 248, 0xdce3e8ff, line);
        if (g_library == LIBRARY_PS3) {
            snprintf(line, sizeof(line), "TITLE ID: %.12s",
                     selected->title_id[0] ? selected->title_id : "NOT READ");
        } else {
            snprintf(line, sizeof(line), "PS2 SERIAL: %.12s",
                     selected->disc_id[0] ? selected->disc_id :
                     (selected->title_id[0] ? selected->title_id : "NOT READ"));
        }
        draw_text(570, 278, 0xdce3e8ff, line);
        draw_text(570, 320, 0x9aa6adff, "CROSS: OPEN GAME DETAILS");
        if (g_cover_texture)
            ya2d_drawTextureEx(g_cover_texture, 690.0f, 220.0f, 0.5f, 120.0f, 160.0f);
        else
            draw_text(690, 230, 0x6f7b83ff, "NO LOCAL COVER");
    } else {
        draw_text(570, 218, 0x9aa6adff, "No matching games found.");
        draw_text(570, 248, 0x9aa6adff, "Press SELECT to search.");
    }

    if (g_config_preview && selected && selected->kind == PS3SCALE_GAME_PS2_ISO) {
        uint8_t config_data[32];
        size_t config_size = 0;
        if (ps3scale_ps2_config_build(
                config_data, sizeof(config_data),
                PS3SCALE_PS2_CFG_DISABLE_SMOOTHING |
                PS3SCALE_PS2_CFG_ENABLE_L2H, &config_size) == 0) {
            draw_text(570, 388, 0xcaff39ff, "PS2 CONFIG PREVIEW ONLY");
            snprintf(line, sizeof(line), "SIZE: %u BYTES",
                     (unsigned)config_size);
            draw_text(570, 416, 0xdce3e8ff, line);
        }
    }

    if (g_games.truncated)
        draw_text(28, 442, 0xffcc66ff, "WARNING: LIBRARY LIMIT REACHED (256 TITLES)");

    draw_text(28, 470, 0x9aa6adff,
              "L1/R1: PLATFORM  SELECT: SEARCH  UP/DOWN: SELECT  CROSS: DETAILS  SQUARE: RESCAN");
    draw_text(28, 498, 0x6f7b83ff,
              "TRIANGLE: PS2 CONFIG PREVIEW  CIRCLE: BACK");
}

static void draw_details(void)
{
    char line[256];
    const ps3scale_game_t *game = selected_game();

    draw_text(28, 22, 0xcaff39ff, "PS3 SCALE");
    draw_text(180, 25, 0x9aa6adff, "GAME DETAILS");

    if (!game) {
        draw_text(34, 120, 0x9aa6adff, "No game selected.");
        draw_text(34, 470, 0x9aa6adff, "CIRCLE: BACK");
        return;
    }

    snprintf(line, sizeof(line), "TITLE: %.100s", game->title);
    draw_text(34, 100, 0xffffffff, line);
    snprintf(line, sizeof(line), "TYPE: %.50s", ps3scale_game_kind_name(game->kind));
    draw_text(34, 140, 0xdce3e8ff, line);

    if (game_is_ps3(game)) {
        snprintf(line, sizeof(line), "TITLE ID: %.16s",
                 game->title_id[0] ? game->title_id : "NOT READ");
        draw_text(34, 180, 0xdce3e8ff, line);
    } else {
        snprintf(line, sizeof(line), "PS2 SERIAL: %.16s",
                 game->disc_id[0] ? game->disc_id :
                 (game->title_id[0] ? game->title_id : "NOT READ"));
        draw_text(34, 180, 0xdce3e8ff, line);
    }

    snprintf(line, sizeof(line), "PATH: %.110s", game->path);
    draw_text(34, 220, 0x9aa6adff, line);

    draw_text(34, 292, 0xffffffff, "GRAPHICS PROFILE");
    draw_text(34, 330, 0xffcc66ff, "NO VERIFIED PATCH MATCHED YET");
    draw_text(34, 362, 0x9aa6adff,
              "The original game files have not been changed.");
    draw_text(34, 394, 0x9aa6adff,
              "Online matching, verified download and rollback are next stages.");

    if (game->kind == PS3SCALE_GAME_PS2_ISO) {
        draw_text(34, 430, 0xcaff39ff, "TRIANGLE: PREVIEW PS2 CONFIG (NO FILE WRITES)");
        if (g_config_preview) {
            uint8_t config_data[32];
            size_t config_size = 0;
            char hex[96];
            if (ps3scale_ps2_config_build(
                    config_data, sizeof(config_data),
                    PS3SCALE_PS2_CFG_DISABLE_SMOOTHING |
                    PS3SCALE_PS2_CFG_ENABLE_L2H, &config_size) == 0) {
                format_hex(hex, sizeof(hex), config_data, config_size);
                draw_text(34, 454, 0xdce3e8ff, hex);
            }
        }
    }

    if (g_cover_texture)
        ya2d_drawTextureEx(g_cover_texture, 690.0f, 120.0f, 0.5f, 120.0f, 160.0f);
    else
        draw_text(690, 140, 0x6f7b83ff, "NO LOCAL COVER");
    if (g_config_preview && game->kind == PS3SCALE_GAME_PS2_ISO) {
        uint8_t config_data[32];
        size_t config_size = 0;
        if (ps3scale_ps2_config_build(
                config_data, sizeof(config_data),
                PS3SCALE_PS2_CFG_DISABLE_SMOOTHING |
                PS3SCALE_PS2_CFG_ENABLE_L2H, &config_size) == 0) {
            char hex[96];
            draw_text(34, 430, 0xcaff39ff, "PS2 CONFIG PREVIEW ONLY - NOT APPLIED");
            format_hex(hex, sizeof(hex), config_data, config_size);
            draw_text(34, 454, 0xdce3e8ff, hex);
        }
    }
    draw_text(34, 478, 0x9aa6adff, "TRIANGLE: CONFIG PREVIEW   CIRCLE: BACK TO GAME LIST");
}

static void draw_settings(void)
{
    draw_text(34, 28, 0xcaff39ff, "PS3 SCALE");
    draw_text(34, 70, 0xffffffff, "SETTINGS");
    draw_text(34, 132, 0xdce3e8ff,
              "Startup theme: GTA V - Greyhound Pause Menu Theme Remix");
    draw_text(34, 172, 0xffcc66ff,
              "Audio playback backend is not enabled in this build yet.");
    draw_text(34, 236, 0xdce3e8ff, "Safety: game files are read-only by default.");
    draw_text(34, 274, 0xdce3e8ff,
              "Any future patch must have a verified backup and rollback.");
    draw_text(34, 478, 0x9aa6adff, "CIRCLE: BACK");
}

static void draw_about(void)
{
    draw_text(34, 28, 0xcaff39ff, "PS3 SCALE");
    draw_text(34, 70, 0xffffffff, "ABOUT");
    draw_text(34, 132, 0xdce3e8ff, "PS3 and PS2 game library and graphics profile manager.");
    draw_text(34, 172, 0xdce3e8ff, "Target: PS3 Slim / CEX CFW.");
    draw_text(34, 212, 0x9aa6adff, "Alpha 0.2 - experimental homebrew build.");
    draw_text(34, 252, 0x9aa6adff,
              "No universal upscaler or frame generation is claimed.");
    draw_text(34, 478, 0x9aa6adff, "CIRCLE: BACK");
}

static void draw_ui(void)
{
    tiny3d_Clear(0xff070a0d, TINY3D_CLEAR_ALL);
    tiny3d_Project2D();

    switch (g_screen) {
        case SCREEN_HOME:     draw_home(); break;
        case SCREEN_SCAN:     draw_scan(); break;
        case SCREEN_DETAILS:  draw_details(); break;
        case SCREEN_SETTINGS: draw_settings(); break;
        case SCREEN_ABOUT:    draw_about(); break;
        default:              draw_home(); break;
    }

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
    u16 old_triangle = 0, old_select = 0;
    int pad_ready = 0;

    (void)argc;
    (void)argv;

    ps3scale_game_list_init(&g_games);

    if (!ya2d_init()) return 1;
    pad_ready = 1;
    g_font_texture = tiny3d_AllocTexture(2 * 1024 * 1024);
    if (!g_font_texture) return 1;

    ResetFont();
    AddFontFromBitmapArray((u8 *)ps3scale_font8x12,
                           (u8 *)g_font_texture,
                           32, 126, 8, 12, 1, BIT7_FIRST_PIXEL);
    SetCurrentFont(0);
    SetFontSize(14, 22);

    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, app_sysutil_callback, NULL);

    scan_library();

    while (!g_exit_requested) {
        u16 up = 0, down = 0, square = 0, cross = 0, circle = 0;
        u16 l1 = 0, r1 = 0, triangle = 0, select = 0;

        sysUtilCheckCallback();

        if (!g_osk_active && pad_ready && ioPadGetInfo(&pad_info) == 0) {
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
                    triangle = pad_data.BTN_TRIANGLE;
                    select = pad_data.BTN_SELECT;
                    break;
                }
            }
        }

        if (!g_osk_active) {
            if (g_screen == SCREEN_HOME) {
                if (up && !old_up)
                    g_home_selected = (g_home_selected + 2) % 3;
                if (down && !old_down)
                    g_home_selected = (g_home_selected + 1) % 3;
                if (cross && !old_cross) {
                    if (g_home_selected == 0) {
                        g_screen = SCREEN_SCAN;
                        g_selected = 0;
                    } else if (g_home_selected == 1) {
                        g_screen = SCREEN_SETTINGS;
                    } else {
                        g_screen = SCREEN_ABOUT;
                    }
                }
                if (circle && !old_circle)
                    g_exit_requested = 1;
            } else if (g_screen == SCREEN_SCAN) {
                if (l1 && !old_l1) {
                    g_library = LIBRARY_PS3;
                    g_selected = 0;
                    g_config_preview = 0;
                }
                if (r1 && !old_r1) {
                    g_library = LIBRARY_PS2;
                    g_selected = 0;
                    g_config_preview = 0;
                }
                if (up && !old_up && library_count(g_library)) {
                    int count = (int)library_count(g_library);
                    g_selected = (g_selected > 0) ? g_selected - 1 : count - 1;
                }
                if (down && !old_down && library_count(g_library))
                    g_selected = (g_selected + 1) % (int)library_count(g_library);
                if (select && !old_select)
                    (void)open_search_keyboard();
                if (square && !old_square)
                    scan_library();
                if (triangle && !old_triangle && g_library == LIBRARY_PS2)
                    g_config_preview = !g_config_preview;
                if (cross && !old_cross && selected_game())
                    g_screen = SCREEN_DETAILS;
                if (circle && !old_circle)
                    g_screen = SCREEN_HOME;
            } else if (g_screen == SCREEN_DETAILS) {
                if (triangle && !old_triangle && selected_game() &&
                    selected_game()->kind == PS3SCALE_GAME_PS2_ISO)
                    g_config_preview = !g_config_preview;
                if (circle && !old_circle)
                    g_screen = SCREEN_SCAN;
            } else {
                if (circle && !old_circle)
                    g_screen = SCREEN_HOME;
            }
        }

        old_up = up;
        old_down = down;
        old_square = square;
        old_cross = cross;
        old_circle = circle;
        old_l1 = l1;
        old_r1 = r1;
        old_triangle = triangle;
        old_select = select;

        if (!g_osk_active) {
            if (g_screen == SCREEN_SCAN || g_screen == SCREEN_DETAILS)
                update_selected_cover();
            draw_ui();
        }
    }

    if (g_osk_active)
        (void)oskAbort();
    sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
    if (g_cover_texture) ya2d_freeTexture(g_cover_texture);
    g_cover_texture = NULL;
    if (pad_ready) ya2d_deinit();
    return 0;
}
