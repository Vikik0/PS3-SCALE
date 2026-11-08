#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <audio/audio.h>
#include <sys/event_queue.h>
#include <sys/thread.h>
#include <soundlib/mpg123.h>

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
    SCREEN_TOOLS,
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

#define PS3SCALE_MUSIC_PATH "/dev_hdd0/game/PS3SCALE1/USRDIR/theme.mp3"
#define PS3SCALE_MUSIC_ORIGINAL_PATH "/dev_hdd0/game/PS3SCALE1/USRDIR/GTA V - Greyhound Pause Menu Theme Remix.mp3"
#define PS3SCALE_MUSIC_USB_PATH "/dev_usb000/PS3SCALE/theme.mp3"

static audioPortParam g_audio_params;
static audioPortConfig g_audio_config;
static sys_event_queue_t g_audio_queue;
static sys_ipc_key_t g_audio_queue_key;
static sys_ppu_thread_t g_audio_thread;
static mpg123_handle *g_mpg123 = NULL;
static u32 g_audio_port = 0;
static int g_audio_initialized = 0;
static int g_audio_port_open = 0;
static int g_audio_port_started = 0;
static int g_audio_queue_ready = 0;
static int g_audio_notify_ready = 0;
static int g_audio_thread_started = 0;
static int g_mpg123_initialized = 0;
static volatile int g_audio_thread_running = 0;
static volatile int g_music_enabled = 1;
static volatile int g_music_volume = 180;
static int g_music_playing = 0;
static int g_settings_selected = 0;
static int g_animation_enabled = 1;
static unsigned long g_frame_counter = 0;
static char g_settings_message[128] = "SETTINGS ARE STORED ON THE PS3 HDD.";
#define SETTINGS_COUNT 7
#define PS3SCALE_SETTINGS_PATH "/dev_hdd0/game/PS3SCALE1/USRDIR/settings.cfg"
static char g_music_path[PS3SCALE_PATH_MAX] = "";

/* PS3 system on-screen keyboard state used by the Scan Games search field. */
static int g_osk_active = 0;
static sys_mem_container_t g_osk_container;
static u16 g_osk_title[64];
static u16 g_osk_start[64];
static u16 g_osk_text[64];
static int g_osk_input_received = 0;
static int g_osk_cancelled = 0;
static int g_osk_unload_requested = 0;
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

static void clamp_selection(void);
static void scan_library(void);

static void app_sysutil_callback(u64 status, u64 param, void *userdata)
{
    (void)param; (void)userdata;
    if (status == SYSUTIL_EXIT_GAME) g_exit_requested = 1;
}

static void osk_sysutil_callback(u64 status, u64 param, void *userdata)
{
    (void)param; (void)userdata;
    switch ((u32)status) {
        case SYSUTIL_OSK_INPUT_ENTERED:
            if (oskGetInputText(&g_osk_result) == 0) g_osk_input_received = 1;
            break;
        case SYSUTIL_OSK_DONE:
            if (!g_osk_input_received && oskGetInputText(&g_osk_result) == 0)
                g_osk_input_received = 1;
            if (!g_osk_unload_requested) {
                g_osk_unload_requested = 1;
                (void)oskUnloadAsync(&g_osk_result);
            }
            break;
        case SYSUTIL_OSK_INPUT_CANCELED:
            g_osk_cancelled = 1;
            if (!g_osk_unload_requested) {
                g_osk_unload_requested = 1;
                (void)oskAbort();
                (void)oskUnloadAsync(&g_osk_result);
            }
            break;
        case SYSUTIL_OSK_UNLOADED:
            if (!g_osk_cancelled && g_osk_input_received &&
                (g_osk_result.res == OSK_OK || g_osk_result.res == OSK_NO_TEXT)) {
                utf16_to_ascii(g_osk_result.str, g_search, sizeof(g_search));
                g_selected = 0;
                g_config_preview = 0;
                clamp_selection();
            }
            g_osk_active = 0;
            (void)sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT1);
            (void)sysMemContainerDestroy(g_osk_container);
            break;
        default: break;
    }
}

static int open_search_keyboard(void)
{
    int rc;
    int max_length = (int)(sizeof(g_osk_text) / sizeof(g_osk_text[0])) - 1;
    if (g_osk_active) return 0;

    memset(g_osk_title, 0, sizeof(g_osk_title));
    memset(g_osk_start, 0, sizeof(g_osk_start));
    memset(g_osk_text, 0, sizeof(g_osk_text));
    memset(&g_osk_input, 0, sizeof(g_osk_input));
    memset(&g_osk_params, 0, sizeof(g_osk_params));
    memset(&g_osk_result, 0, sizeof(g_osk_result));

    ascii_to_utf16("Search games by title:", g_osk_title,
                   sizeof(g_osk_title) / sizeof(g_osk_title[0]));
    ascii_to_utf16(g_search, g_osk_start,
                   sizeof(g_osk_start) / sizeof(g_osk_start[0]));

    g_osk_input.message = g_osk_title;
    g_osk_input.startText = g_osk_start;
    g_osk_input.maxLength = max_length;
    g_osk_params.allowedPanels = OSK_PANEL_TYPE_ALPHABET |
        OSK_PANEL_TYPE_NUMERAL | OSK_PANEL_TYPE_ENGLISH | OSK_PANEL_TYPE_URL;
    g_osk_params.firstViewPanel = OSK_PANEL_TYPE_ALPHABET_FULL_WIDTH;
    g_osk_params.controlPoint.x = 0.0f;
    g_osk_params.controlPoint.y = 0.0f;
    g_osk_params.prohibitFlags = OSK_PROHIBIT_RETURN;

    g_osk_result.res = OSK_NO_TEXT;
    g_osk_result.len = max_length;
    g_osk_result.str = g_osk_text;
    g_osk_input_received = 0;
    g_osk_cancelled = 0;
    g_osk_unload_requested = 0;

    rc = sysMemContainerCreate(&g_osk_container, 4 * 1024 * 1024);
    if (rc != 0) return rc;

    if (oskSetKeyLayoutOption(OSK_10KEY_PANEL | OSK_FULLKEY_PANEL) != 0 ||
        oskAddSupportLanguage(g_osk_params.allowedPanels) != 0 ||
        oskSetInitialInputDevice(OSK_DEVICE_PAD) != 0 ||
        oskSetLayoutMode(OSK_LAYOUTMODE_HORIZONTAL_ALIGN_CENTER |
                         OSK_LAYOUTMODE_VERTICAL_ALIGN_CENTER) != 0) {
        (void)sysMemContainerDestroy(g_osk_container);
        return -1;
    }

    rc = sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT1, osk_sysutil_callback, NULL);
    if (rc != 0) {
        (void)sysMemContainerDestroy(g_osk_container);
        return rc;
    }
    rc = oskLoadAsync(g_osk_container, &g_osk_params, &g_osk_input);
    if (rc != 0) {
        (void)sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT1);
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

static int open_background_music(void)
{
    static const char *paths[] = {
        PS3SCALE_MUSIC_PATH,
        PS3SCALE_MUSIC_ORIGINAL_PATH,
        "/dev_usb000/PS3SCALE/theme.mp3",
        "/dev_usb001/PS3SCALE/theme.mp3",
        "/dev_usb002/PS3SCALE/theme.mp3",
        "/dev_usb003/PS3SCALE/theme.mp3",
        "/dev_usb004/PS3SCALE/theme.mp3",
        "/dev_usb005/PS3SCALE/theme.mp3",
        "/dev_usb006/PS3SCALE/theme.mp3",
        "/dev_usb007/PS3SCALE/theme.mp3"
    };
    int error = 0;
    size_t i;

    if (mpg123_init() != MPG123_OK)
        return -1;
    g_mpg123_initialized = 1;

    g_mpg123 = mpg123_new(NULL, &error);
    if (!g_mpg123)
        return -1;

    if (mpg123_format_none(g_mpg123) != MPG123_OK ||
        mpg123_param(g_mpg123, MPG123_FORCE_RATE, 48000, 0.0) != MPG123_OK ||
        mpg123_format(g_mpg123, 48000, MPG123_STEREO,
                      MPG123_ENC_SIGNED_16) != MPG123_OK)
        return -1;

    for (i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        long rate = 0;
        int channels = 0;
        int encoding = 0;

        if (mpg123_open(g_mpg123, paths[i]) != MPG123_OK)
            continue;

        if (mpg123_getformat(g_mpg123, &rate, &channels, &encoding) == MPG123_OK &&
            rate == 48000 && channels == MPG123_STEREO &&
            encoding == MPG123_ENC_SIGNED_16) {
            strncpy(g_music_path, paths[i], sizeof(g_music_path) - 1);
            g_music_path[sizeof(g_music_path) - 1] = '\0';
            return 0;
        }

        mpg123_close(g_mpg123);
    }

    return -1;
}

static void fill_audio_block(float *output)
{
    short pcm[AUDIO_BLOCK_SAMPLES * 2];
    size_t total = 0, target = sizeof(pcm), i;
    float gain = g_music_enabled ? ((float)g_music_volume / 255.0f) : 0.0f;

    if (gain <= 0.0f) {
        memset(output, 0, sizeof(float) * AUDIO_BLOCK_SAMPLES * 2);
        return;
    }
    memset(pcm, 0, sizeof(pcm));
    while (total < target && g_audio_thread_running) {
        size_t decoded = 0;
        int rc = mpg123_read(g_mpg123, ((unsigned char *)pcm) + total,
                             target - total, &decoded);
        total += decoded;
        if (rc == MPG123_NEW_FORMAT) {
            long rate = 0; int channels = 0, encoding = 0;
            if (mpg123_getformat(g_mpg123, &rate, &channels, &encoding) != MPG123_OK ||
                rate != 48000 || channels != MPG123_STEREO ||
                encoding != MPG123_ENC_SIGNED_16) break;
            continue;
        }
        if (rc == MPG123_DONE) {
            if (mpg123_seek(g_mpg123, 0, SEEK_SET) < 0) break;
            continue;
        }
        if (rc == MPG123_ERR || (decoded == 0 && rc == MPG123_OK)) break;
    }
    for (i = 0; i < AUDIO_BLOCK_SAMPLES * 2; i++)
        output[i] = ((float)pcm[i] / 32768.0f) * gain;
}

static void audio_output_thread(void *arg)
{
    (void)arg;

    while (g_audio_thread_running) {
        sys_event_t event;
        u64 current_block;
        u32 next_block;
        float *audio_data;
        float *destination;
        s32 rc = sysEventQueueReceive(g_audio_queue, &event, 20 * 1000);

        if (rc != 0)
            continue;
        if (!g_audio_thread_running)
            break;

        current_block = *(volatile u64 *)(uintptr_t)g_audio_config.readIndex;
        next_block = (u32)((current_block + 1) % g_audio_config.numBlocks);
        audio_data = (float *)(uintptr_t)g_audio_config.audioDataStart;
        destination = audio_data +
            ((size_t)next_block * (size_t)g_audio_config.channelCount *
             AUDIO_BLOCK_SAMPLES);

        fill_audio_block(destination);
    }
}

static void set_background_music_enabled(int enabled)
{
    g_music_enabled = enabled ? 1 : 0;
}

static void set_background_music_volume(int volume)
{
    if (volume < 0) volume = 0;
    if (volume > 255) volume = 255;
    g_music_volume = volume;
}

static void shutdown_audio_backend(void)
{
    u64 thread_result = 0;

    g_audio_thread_running = 0;

    if (g_audio_thread_started) {
        (void)sysThreadJoin(g_audio_thread, &thread_result);
        g_audio_thread_started = 0;
    }

    if (g_audio_port_started) {
        (void)audioPortStop(g_audio_port);
        g_audio_port_started = 0;
    }

    if (g_audio_notify_ready) {
        (void)audioRemoveNotifyEventQueue(g_audio_queue_key);
        g_audio_notify_ready = 0;
    }

    if (g_audio_port_open) {
        (void)audioPortClose(g_audio_port);
        g_audio_port_open = 0;
    }

    if (g_audio_queue_ready) {
        (void)sysEventQueueDestroy(g_audio_queue, 0);
        g_audio_queue_ready = 0;
    }

    if (g_audio_initialized) {
        (void)audioQuit();
        g_audio_initialized = 0;
    }

    if (g_mpg123) {
        (void)mpg123_close(g_mpg123);
        mpg123_delete(g_mpg123);
        g_mpg123 = NULL;
    }

    if (g_mpg123_initialized) {
        mpg123_exit();
        g_mpg123_initialized = 0;
    }

    g_music_playing = 0;
}

static int initialize_audio_backend(void)
{
    int rc;

    if (open_background_music() != 0)
        goto fail;

    rc = audioInit();
    if (rc != 0)
        goto fail;
    g_audio_initialized = 1;

    g_audio_params.numChannels = AUDIO_PORT_2CH;
    g_audio_params.numBlocks = AUDIO_BLOCK_8;
    g_audio_params.attrib = AUDIO_PORT_INITLEVEL;
    g_audio_params.level = 1.0f;

    rc = audioPortOpen(&g_audio_params, &g_audio_port);
    if (rc != 0)
        goto fail;
    g_audio_port_open = 1;

    rc = audioGetPortConfig(g_audio_port, &g_audio_config);
    if (rc != 0)
        goto fail;

    rc = audioCreateNotifyEventQueue(&g_audio_queue, &g_audio_queue_key);
    if (rc != 0)
        goto fail;
    g_audio_queue_ready = 1;

    rc = audioSetNotifyEventQueue(g_audio_queue_key);
    if (rc != 0)
        goto fail;
    g_audio_notify_ready = 1;

    (void)sysEventQueueDrain(g_audio_queue);

    rc = audioPortStart(g_audio_port);
    if (rc != 0)
        goto fail;
    g_audio_port_started = 1;

    g_audio_thread_running = 1;
    rc = sysThreadCreate(&g_audio_thread, audio_output_thread, NULL,
                         1000, 0x4000, THREAD_JOINABLE, "PS3 SCALE Music");
    if (rc != 0) {
        g_audio_thread_running = 0;
        goto fail;
    }

    g_audio_thread_started = 1;
    g_music_playing = 1;
    return 0;

fail:
    shutdown_audio_backend();
    return -1;
}

static void draw_solid_rect(float x, float y, float w, float h, u32 color)
{
    tiny3d_SetPolygon(TINY3D_QUADS);
    tiny3d_VertexPos(x, y, 65535.0f);
    tiny3d_VertexColor(color);
    tiny3d_VertexPos(x + w, y, 65535.0f);
    tiny3d_VertexPos(x + w, y + h, 65535.0f);
    tiny3d_VertexPos(x, y + h, 65535.0f);
    tiny3d_End();
}

static void draw_outline_rect(float x, float y, float w, float h, u32 color)
{
    tiny3d_SetPolygon(TINY3D_LINE_LOOP);
    tiny3d_VertexPos(x, y, 65535.0f);
    tiny3d_VertexColor(color);
    tiny3d_VertexPos(x + w, y, 65535.0f);
    tiny3d_VertexPos(x + w, y + h, 65535.0f);
    tiny3d_VertexPos(x, y + h, 65535.0f);
    tiny3d_End();
}

static void draw_icon_line(float x1, float y1, float x2, float y2, u32 color)
{
    tiny3d_SetPolygon(TINY3D_LINES);
    tiny3d_VertexPos(x1, y1, 65535.0f);
    tiny3d_VertexColor(color);
    tiny3d_VertexPos(x2, y2, 65535.0f);
    tiny3d_End();
}

static void draw_circle_outline(float cx, float cy, float radius, u32 color)
{
    int i;
    tiny3d_SetPolygon(TINY3D_LINE_LOOP);
    for (i = 0; i < 24; i++) {
        float angle = 6.28318530718f * (float)i / 24.0f;
        tiny3d_VertexPos(cx + cosf(angle) * radius,
                         cy + sinf(angle) * radius, 65535.0f);
        if (i == 0)
            tiny3d_VertexColor(color);
    }
    tiny3d_End();
}

static void draw_gear_outline(float cx, float cy, float radius, u32 color)
{
    int i;
    tiny3d_SetPolygon(TINY3D_LINE_LOOP);
    for (i = 0; i < 24; i++) {
        float angle = 6.28318530718f * (float)i / 24.0f;
        float r = (i % 3 == 0) ? radius : radius * 0.78f;
        tiny3d_VertexPos(cx + cosf(angle) * r,
                         cy + sinf(angle) * r, 65535.0f);
        if (i == 0)
            tiny3d_VertexColor(color);
    }
    tiny3d_End();
}

static void draw_menu_icon(int index, float x, float y, int selected)
{
    u32 ink = selected ? 0xcaff39ff : 0x9eb2c0ff;
    u32 tile = selected ? 0x1b2b1dff : 0x111b25ff;
    u32 border = selected ? 0x8cbd35ff : 0x344654ff;
    draw_solid_rect(x, y, 38.0f, 38.0f, tile);
    draw_outline_rect(x, y, 38.0f, 38.0f, border);
    if (index == 0) {
        draw_circle_outline(x + 15.0f, y + 14.0f, 8.0f, ink);
        draw_icon_line(x + 21.0f, y + 21.0f, x + 29.0f, y + 29.0f, ink);
        draw_icon_line(x + 11.0f, y + 12.0f, x + 17.0f, y + 12.0f, ink);
        draw_icon_line(x + 11.0f, y + 16.0f, x + 16.0f, y + 16.0f, ink);
    } else if (index == 1) {
        draw_gear_outline(x + 19.0f, y + 19.0f, 13.0f, ink);
        draw_circle_outline(x + 19.0f, y + 19.0f, 5.0f, ink);
        draw_solid_rect(x + 17.5f, y + 17.5f, 3.0f, 3.0f, ink);
    } else if (index == 2) {
        draw_icon_line(x + 10.0f, y + 28.0f, x + 27.0f, y + 11.0f, ink);
        draw_icon_line(x + 11.0f, y + 10.0f, x + 28.0f, y + 27.0f, ink);
        draw_circle_outline(x + 10.0f, y + 10.0f, 4.0f, ink);
        draw_circle_outline(x + 28.0f, y + 28.0f, 4.0f, ink);
    } else {
        draw_circle_outline(x + 19.0f, y + 19.0f, 13.0f, ink);
        draw_solid_rect(x + 17.5f, y + 16.0f, 3.0f, 10.0f, ink);
        draw_solid_rect(x + 17.5f, y + 10.0f, 3.0f, 3.0f, ink);
    }
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

static size_t library_total_count(int library)
{
    size_t i, count = 0;
    for (i = 0; i < g_games.count; i++) {
        int match = (library == LIBRARY_PS3) ?
            game_is_ps3(&g_games.items[i]) : game_is_ps2(&g_games.items[i]);
        if (match) count++;
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

static void draw_home_ambient(void)
{
    float phase = g_animation_enabled ? (float)g_frame_counter * 0.0024f : 0.0f;
    float cx = 670.0f, cy = 275.0f;
    int i;
    draw_circle_outline(cx, cy, 112.0f + 3.0f * sinf(phase), 0x1b3b56ff);
    draw_circle_outline(cx, cy, 78.0f + 2.0f * cosf(phase * 0.7f), 0x173149ff);
    for (i = 0; i < 4; i++) {
        float angle = phase + (float)i * 1.57079632679f;
        float px = cx + cosf(angle) * 112.0f;
        float py = cy + sinf(angle) * 112.0f;
        draw_solid_rect(px - 2.0f, py - 2.0f, 4.0f, 4.0f, 0x477b9cff);
    }
    draw_icon_line(500.0f, 400.0f, 808.0f, 400.0f, 0x18334aff);
}

static void set_settings_message(const char *message)
{
    if (!message) message = "";
    snprintf(g_settings_message, sizeof(g_settings_message), "%s", message);
}

static void load_app_settings(void)
{
    FILE *fp = fopen(PS3SCALE_SETTINGS_PATH, "r");
    char line[64]; int value;
    if (!fp) return;
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "music=%d", &value) == 1)
            g_music_enabled = value ? 1 : 0;
        else if (sscanf(line, "volume=%d", &value) == 1)
            set_background_music_volume(value);
        else if (sscanf(line, "animation=%d", &value) == 1)
            g_animation_enabled = value ? 1 : 0;
    }
    fclose(fp);
}

static int save_app_settings(void)
{
    FILE *fp = fopen(PS3SCALE_SETTINGS_PATH, "w");
    if (!fp) {
        set_settings_message("SETTINGS COULD NOT BE SAVED TO INTERNAL HDD.");
        return -1;
    }
    fprintf(fp, "music=%d\n", g_music_enabled ? 1 : 0);
    fprintf(fp, "volume=%d\n", g_music_volume);
    fprintf(fp, "animation=%d\n", g_animation_enabled ? 1 : 0);
    if (fclose(fp) != 0) {
        set_settings_message("SETTINGS SAVE FAILED.");
        return -1;
    }
    set_settings_message("SETTINGS SAVED TO INTERNAL HDD.");
    return 0;
}

static int find_usb_update_package(char *found_path, size_t capacity)
{
    static const char *relative_paths[] = {
        "PS3SCALE/update/ps3scale.gnpdrm.pkg",
        "PS3SCALE/update/ps3scale.pkg",
        "PS3SCALE/ps3scale.gnpdrm.pkg"
    };
    unsigned char expected[4] = { 0x7f, 'P', 'K', 'G' };
    int device, item;
    for (device = 0; device < 8; device++) {
        char root[32];
        snprintf(root, sizeof(root), "/dev_usb%03d", device);
        for (item = 0; item < 3; item++) {
            char candidate[PS3SCALE_PATH_MAX];
            unsigned char magic[4];
            FILE *fp;
            if (snprintf(candidate, sizeof(candidate), "%s/%s",
                         root, relative_paths[item]) >= (int)sizeof(candidate))
                continue;
            fp = fopen(candidate, "rb");
            if (!fp) continue;
            if (fread(magic, 1, sizeof(magic), fp) == sizeof(magic) &&
                memcmp(magic, expected, sizeof(expected)) == 0) {
                fclose(fp);
                if (found_path && capacity > 0)
                    snprintf(found_path, capacity, "%s", candidate);
                return 1;
            }
            fclose(fp);
        }
    }
    return 0;
}

static int copy_music_to_internal(void)
{
    FILE *existing = fopen(PS3SCALE_MUSIC_PATH, "rb");
    FILE *input = NULL;
    FILE *output = NULL;
    char source_path[PS3SCALE_PATH_MAX];
    unsigned char buffer[16384];
    size_t amount;
    int device, failed = 0;

    if (existing) {
        fclose(existing);
        return 1;
    }

    source_path[0] = '\0';
    if (g_music_path[0] && strstr(g_music_path, "/dev_usb")) {
        snprintf(source_path, sizeof(source_path), "%s", g_music_path);
        input = fopen(source_path, "rb");
    }

    for (device = 0; !input && device < 8; device++) {
        snprintf(source_path, sizeof(source_path),
                 "/dev_usb%03d/PS3SCALE/theme.mp3", device);
        input = fopen(source_path, "rb");
    }

    if (!input)
        return -1;

    output = fopen(PS3SCALE_MUSIC_PATH, "wb");
    if (!output) {
        fclose(input);
        return -1;
    }

    while ((amount = fread(buffer, 1, sizeof(buffer), input)) > 0) {
        if (fwrite(buffer, 1, amount, output) != amount) {
            failed = 1;
            break;
        }
    }

    if (ferror(input))
        failed = 1;
    if (fclose(input) != 0)
        failed = 1;
    if (fclose(output) != 0)
        failed = 1;

    if (failed) {
        (void)remove(PS3SCALE_MUSIC_PATH);
        return -1;
    }

    return 0;
}

static void draw_home(void)
{
    static char *items[] = {
        "SCAN GAMES", "SETTINGS", "COMPANION TOOLS", "ABOUT PS3 SCALE"
    };
    char line[128];
    size_t ps3_count = library_total_count(LIBRARY_PS3);
    size_t ps2_count = library_total_count(LIBRARY_PS2);
    int i;
    SetFontSize(12, 18);
    draw_home_ambient();
    draw_text(34, 26, 0xcaff39ff, "PS3 SCALE");
    draw_text(34, 58, 0x9aa6adff,
              "GAME LIBRARY  /  GRAPHICS PROFILES  /  SAFE TOOLS");
    draw_text(34, 112, 0xffffffff, "MAIN MENU");
    for (i = 0; i < 4; i++) {
        float row_y = 154.0f + (float)i * 58.0f;
        int selected = (i == g_home_selected);
        if (selected) draw_solid_rect(48.0f, row_y, 408.0f, 44.0f, 0x111e18ff);
        draw_menu_icon(i, 66.0f, row_y + 3.0f, selected);
        if (selected) draw_text(44, row_y + 13.0f, 0xcaff39ff, ">");
        draw_text(124, row_y + 13.0f,
                  selected ? 0xcaff39ff : 0xdce3e8ff, items[i]);
    }
    draw_outline_rect(490.0f, 126.0f, 326.0f, 302.0f, 0x26445cff);
    draw_text(520, 148, 0xffffffff, "LIBRARY OVERVIEW");
    snprintf(line, sizeof(line), "PS3 TITLES: %u", (unsigned)ps3_count);
    draw_text(520, 188, 0xdce3e8ff, line);
    snprintf(line, sizeof(line), "PS2 TITLES: %u", (unsigned)ps2_count);
    draw_text(520, 218, 0xdce3e8ff, line);
    snprintf(line, sizeof(line), "TOTAL TITLES: %u",
             (unsigned)(ps3_count + ps2_count));
    draw_text(520, 248, 0xcaff39ff, line);
    draw_text(520, 302, 0xffffffff, "SYSTEM STATUS");
    snprintf(line, sizeof(line), "MUSIC: %s", g_music_enabled ? "ON" : "OFF");
    draw_text(520, 332, 0xdce3e8ff, line);
    snprintf(line, sizeof(line), "HOME ANIMATION: %s",
             g_animation_enabled ? "ON" : "OFF");
    draw_text(520, 362, 0xdce3e8ff, line);
    draw_text(520, 392, 0x9aa6adff, "GAME FILES: READ-ONLY");
    draw_text(34, 448, 0x9aa6adff,
              "UP/DOWN: SELECT   CROSS: OPEN   CIRCLE: EXIT");
    draw_text(34, 478, 0x6f7b83ff,
              "LOCAL LIBRARY SCAN  |  NO INTERNET REQUIRED");
    SetFontSize(14, 22);
}

static void draw_scan(void)
{
    char line[256];
    size_t i, count, start;
    size_t ps3_count = library_total_count(LIBRARY_PS3);
    size_t ps2_count = library_total_count(LIBRARY_PS2);
    const ps3scale_game_t *selected;
    SetFontSize(10, 16);
    clamp_selection();
    count = library_count(g_library);
    selected = selected_game();

    draw_text(28, 18, 0xcaff39ff, "PS3 SCALE");
    draw_text(178, 18, 0x9aa6adff, "SCAN GAMES");
    draw_text(28, 52, 0xffffffff, "PLATFORM");
    draw_text(150, 52, g_library == LIBRARY_PS3 ? 0xcaff39ff : 0x9aa6adff, "L1: PS3");
    draw_text(250, 52, g_library == LIBRARY_PS2 ? 0xcaff39ff : 0x9aa6adff, "R1: PS2");
    draw_text(360, 52, 0x9aa6adff, "SELECT: SEARCH");
    snprintf(line, sizeof(line), "SEARCH: %.48s",
             g_search[0] ? g_search : "ALL GAMES");
    draw_text(28, 82, 0xdce3e8ff, line);
    snprintf(line, sizeof(line), "PS3: %u  |  PS2: %u  |  RESULTS: %u",
             (unsigned)ps3_count, (unsigned)ps2_count, (unsigned)count);
    draw_text(28, 110, 0x9aa6adff, line);
    draw_text(28, 146, 0xffffffff,
              g_library == LIBRARY_PS3 ? "PS3 GAME LIST" : "PS2 GAME LIST");
    draw_text(530, 146, 0xffffffff, "SELECTED GAME");

    start = (g_selected > 7) ? (size_t)(g_selected - 7) : 0;
    for (i = 0; i < 8 && start + i < count; i++) {
        const ps3scale_game_t *game = library_game_at(start + i);
        int is_selected = ((int)(start + i) == g_selected);
        float row_y = 174.0f + (float)i * 30.0f;
        if (!game) continue;
        if (is_selected) draw_solid_rect(24.0f, row_y - 2.0f, 478.0f, 24.0f, 0x14261dff);
        snprintf(line, sizeof(line), "%c %-10s %.32s",
                 is_selected ? '>' : ' ',
                 ps3scale_game_kind_name(game->kind), game->title);
        draw_text(30, row_y, is_selected ? 0xcaff39ff : 0xdce3e8ff, line);
    }

    if (selected) {
        snprintf(line, sizeof(line), "TITLE: %.28s", selected->title);
        draw_text(530, 176, 0xdce3e8ff, line);
        snprintf(line, sizeof(line), "TYPE: %.20s", ps3scale_game_kind_name(selected->kind));
        draw_text(530, 204, 0xdce3e8ff, line);
        if (g_library == LIBRARY_PS3) {
            snprintf(line, sizeof(line), "TITLE ID: %.16s",
                     selected->title_id[0] ? selected->title_id : "NOT READ");
        } else {
            snprintf(line, sizeof(line), "PS2 SERIAL: %.16s",
                     selected->disc_id[0] ? selected->disc_id :
                     (selected->title_id[0] ? selected->title_id : "NOT READ"));
        }
        draw_text(530, 232, 0xdce3e8ff, line);
        draw_text(530, 260, 0x9aa6adff, "CROSS: OPEN DETAILS");
        if (g_cover_texture)
            ya2d_drawTextureEx(g_cover_texture, 690.0f, 286.0f, 0.5f, 112.0f, 148.0f);
        else {
            draw_solid_rect(690.0f, 286.0f, 112.0f, 148.0f, 0x101b27ff);
            draw_outline_rect(690.0f, 286.0f, 112.0f, 148.0f, 0x344654ff);
            draw_text(700, 350, 0x6f7b83ff, "NO COVER");
        }
    } else {
        draw_text(530, 180, 0x9aa6adff, "NO MATCHING GAMES FOUND.");
        draw_text(530, 208, 0x9aa6adff, "PRESS SELECT TO SEARCH.");
    }

    if (g_config_preview && selected && selected->kind == PS3SCALE_GAME_PS2_ISO) {
        uint8_t config_data[32]; size_t config_size = 0;
        if (ps3scale_ps2_config_build(config_data, sizeof(config_data),
                PS3SCALE_PS2_CFG_DISABLE_SMOOTHING |
                PS3SCALE_PS2_CFG_ENABLE_L2H, &config_size) == 0) {
            snprintf(line, sizeof(line), "PS2 CONFIG PREVIEW: %u BYTES",
                     (unsigned)config_size);
            draw_text(530, 440, 0xcaff39ff, line);
        }
    }
    if (g_games.truncated)
        draw_text(28, 420, 0xffcc66ff, "WARNING: LIBRARY LIMIT REACHED (256 TITLES)");
    draw_text(28, 470, 0x9aa6adff,
              "L1/R1: PLATFORM  SELECT: SEARCH  UP/DOWN: MOVE  CROSS: DETAILS");
    draw_text(28, 492, 0x6f7b83ff,
              "SQUARE: RESCAN  TRIANGLE: PS2 PREVIEW  CIRCLE: BACK");
    SetFontSize(14, 22);
}

static void draw_details(void)
{
    char line[256];
    const ps3scale_game_t *game = selected_game();
    SetFontSize(11, 17);
    draw_text(28, 22, 0xcaff39ff, "PS3 SCALE");
    draw_text(180, 25, 0x9aa6adff, "GAME DETAILS");
    if (!game) {
        draw_text(34, 120, 0x9aa6adff, "NO GAME SELECTED.");
        draw_text(34, 478, 0x9aa6adff, "CIRCLE: BACK");
        SetFontSize(14, 22);
        return;
    }
    snprintf(line, sizeof(line), "TITLE: %.48s", game->title);
    draw_text(34, 96, 0xffffffff, line);
    snprintf(line, sizeof(line), "TYPE: %.42s", ps3scale_game_kind_name(game->kind));
    draw_text(34, 132, 0xdce3e8ff, line);
    if (game_is_ps3(game))
        snprintf(line, sizeof(line), "TITLE ID: %.16s",
                 game->title_id[0] ? game->title_id : "NOT READ");
    else
        snprintf(line, sizeof(line), "PS2 SERIAL: %.16s",
                 game->disc_id[0] ? game->disc_id :
                 (game->title_id[0] ? game->title_id : "NOT READ"));
    draw_text(34, 168, 0xdce3e8ff, line);
    snprintf(line, sizeof(line), "PATH: %.48s", game->path);
    draw_text(34, 204, 0x9aa6adff, line);
    draw_text(34, 260, 0xffffffff, "GRAPHICS PROFILE");
    draw_text(34, 296, 0xffcc66ff, "NO VERIFIED PATCH MATCHED YET");
    draw_text(34, 328, 0x9aa6adff, "ORIGINAL GAME FILES HAVE NOT BEEN CHANGED.");
    draw_text(34, 360, 0x9aa6adff,
              "PROFILE DOWNLOAD / BACKUP / ROLLBACK ARE NOT ACTIVE YET.");
    if (g_cover_texture)
        ya2d_drawTextureEx(g_cover_texture, 690.0f, 112.0f, 0.5f, 120.0f, 160.0f);
    else {
        draw_solid_rect(690.0f, 112.0f, 120.0f, 160.0f, 0x101b27ff);
        draw_outline_rect(690.0f, 112.0f, 120.0f, 160.0f, 0x344654ff);
        draw_text(702, 180, 0x6f7b83ff, "NO COVER");
    }
    if (game->kind == PS3SCALE_GAME_PS2_ISO) {
        draw_text(34, 408, 0xcaff39ff,
                  g_config_preview ? "PS2 CONFIG PREVIEW - NOT APPLIED" :
                                     "TRIANGLE: PREVIEW PS2 CONFIG");
        if (g_config_preview) {
            uint8_t config_data[32]; size_t config_size = 0; char hex[96];
            if (ps3scale_ps2_config_build(config_data, sizeof(config_data),
                    PS3SCALE_PS2_CFG_DISABLE_SMOOTHING |
                    PS3SCALE_PS2_CFG_ENABLE_L2H, &config_size) == 0) {
                format_hex(hex, sizeof(hex), config_data, config_size);
                draw_text(34, 434, 0xdce3e8ff, hex);
            }
        }
    }
    draw_text(34, 478, 0x9aa6adff,
              "TRIANGLE: CONFIG PREVIEW  |  CIRCLE: BACK TO GAME LIST");
    SetFontSize(14, 22);
}

static void draw_settings(void)
{
    static char *labels[SETTINGS_COUNT] = {
        "BACKGROUND MUSIC", "MUSIC VOLUME", "HOME ANIMATION",
        "CHECK FOR USB UPDATE", "RESCAN GAME LIBRARY",
        "RESTORE DEFAULT SETTINGS", "COPY MUSIC TO INTERNAL HDD"
    };
    char value[96];
    int i;
    SetFontSize(12, 18);
    draw_text(34, 24, 0xcaff39ff, "PS3 SCALE");
    draw_text(34, 58, 0xffffffff, "SETTINGS");
    draw_text(34, 92, 0x9aa6adff,
              "PERSONALIZE THE APP AND RUN SAFE MAINTENANCE TASKS.");
    for (i = 0; i < SETTINGS_COUNT; i++) {
        float y = 124.0f + (float)i * 38.0f;
        int selected = (i == g_settings_selected);
        if (selected) draw_solid_rect(28.0f, y - 4.0f, 780.0f, 34.0f, 0x14261dff);
        value[0] = '\0';
        if (i == 0) snprintf(value, sizeof(value), "%s", g_music_enabled ? "ON" : "OFF");
        else if (i == 1) snprintf(value, sizeof(value), "%d%%", (g_music_volume * 100) / 255);
        else if (i == 2) snprintf(value, sizeof(value), "%s", g_animation_enabled ? "ON" : "OFF");
        else if (i == 3) snprintf(value, sizeof(value), "SCAN USB000 - USB007");
        else if (i == 4) snprintf(value, sizeof(value), "CURRENT LIBRARY");
        else if (i == 5) snprintf(value, sizeof(value), "DEFAULTS");
        else snprintf(value, sizeof(value), "IMPORT FROM USB");
        draw_text(48, y, selected ? 0xcaff39ff : 0xdce3e8ff, labels[i]);
        draw_text(590, y, selected ? 0xcaff39ff : 0x9aa6adff, value);
    }
    if (g_music_playing) {
        const char *source = strstr(g_music_path, "/dev_usb") ? "USB" : "INTERNAL HDD";
        snprintf(value, sizeof(value), "AUDIO ENGINE: READY  |  SOURCE: %s", source);
        draw_text(34, 398, 0x9ee56aff, value);
    } else {
        draw_text(34, 398, 0xffcc66ff,
                  "AUDIO ENGINE: MUSIC FILE NOT FOUND OR INIT FAILED");
    }
    draw_text(34, 428, 0xdce3e8ff, g_settings_message);
    draw_text(34, 478, 0x9aa6adff,
              "UP/DOWN: MOVE  CROSS: SELECT  LEFT/RIGHT: ADJUST  CIRCLE: BACK");
    SetFontSize(14, 22);
}

static void draw_tools(void)
{
    SetFontSize(11, 17);
    draw_text(34, 24, 0xcaff39ff, "PS3 SCALE");
    draw_text(34, 62, 0xffffffff, "COMPANION TOOLS");
    draw_text(34, 112, 0xcaff39ff, "APOLLO SAVE TOOL");
    draw_text(34, 140, 0xdce3e8ff,
              "Save data management, backups, resigning, trophies and licenses.");
    draw_text(34, 190, 0xcaff39ff, "ARTEMIS PS3");
    draw_text(34, 218, 0xdce3e8ff,
              "Cheat database and in-game code attachment through its own plugin.");
    draw_text(34, 268, 0xcaff39ff, "webMAN MOD");
    draw_text(34, 296, 0xdce3e8ff,
              "Game mounting, FTP/web tools, fan control and temperature monitoring.");
    draw_text(34, 354, 0xffffffff, "INTEGRATION POLICY");
    draw_text(34, 384, 0x9aa6adff,
              "These are separate homebrew apps; PS3 SCALE does not replace them.");
    draw_text(34, 414, 0x9aa6adff,
              "This build does not load their plugins or alter their data.");
    draw_text(34, 478, 0x9aa6adff, "CIRCLE: BACK");
    SetFontSize(14, 22);
}

static void draw_about(void)
{
    SetFontSize(11, 17);
    draw_text(34, 24, 0xcaff39ff, "PS3 SCALE");
    draw_text(34, 62, 0xffffffff, "ABOUT");
    draw_text(34, 112, 0xdce3e8ff,
              "PS3 / PS2 GAME LIBRARY AND GRAPHICS PROFILE MANAGER.");
    draw_text(34, 152, 0xdce3e8ff,
              "TARGET TEST DEVICE: PS3 SLIM / CEX CFW.");
    draw_text(34, 192, 0x9aa6adff,
              "ALPHA 0.2 - EXPERIMENTAL HOMEBREW SOFTWARE.");
    draw_text(34, 232, 0x9aa6adff,
              "SCANNING IS READ-ONLY. ORIGINAL GAME FILES ARE PRESERVED.");
    draw_text(34, 272, 0x9aa6adff,
              "NO UNIVERSAL UPSCALER OR FRAME GENERATION IS CLAIMED.");
    draw_text(34, 312, 0x9aa6adff,
              "PROFILES NEED EXACT TITLE / UPDATE MATCHING.");
    draw_text(34, 478, 0x9aa6adff, "CIRCLE: BACK");
    SetFontSize(14, 22);
}

static void draw_ui(void)
{
    tiny3d_Clear(0xff070a0d, TINY3D_CLEAR_ALL);
    tiny3d_Project2D();
    switch (g_screen) {
        case SCREEN_HOME: draw_home(); break;
        case SCREEN_SCAN: draw_scan(); break;
        case SCREEN_DETAILS: draw_details(); break;
        case SCREEN_SETTINGS: draw_settings(); break;
        case SCREEN_TOOLS: draw_tools(); break;
        case SCREEN_ABOUT: draw_about(); break;
        default: draw_home(); break;
    }
    tiny3d_Flip();
}

static int compare_game_titles(const void *left, const void *right)
{
    const ps3scale_game_t *a = (const ps3scale_game_t *)left;
    const ps3scale_game_t *b = (const ps3scale_game_t *)right;
    const unsigned char *pa = (const unsigned char *)a->title;
    const unsigned char *pb = (const unsigned char *)b->title;
    while (*pa && *pb) {
        int ca = tolower(*pa), cb = tolower(*pb);
        if (ca != cb) return ca - cb;
        pa++; pb++;
    }
    if (*pa) return 1;
    if (*pb) return -1;
    return strcmp(a->path, b->path);
}

static void scan_library(void)
{
    (void)ps3scale_scan_all(&g_games);
    if (g_games.count > 1)
        qsort(g_games.items, g_games.count, sizeof(g_games.items[0]),
              compare_game_titles);
    g_selected = 0;
    clamp_selection();
}

int main(int argc, char *argv[])
{
    padInfo pad_info;
    padData pad_data;
    u16 old_up = 0, old_down = 0, old_square = 0;
    u16 old_cross = 0, old_circle = 0, old_l1 = 0, old_r1 = 0;
    u16 old_triangle = 0, old_select = 0, old_left = 0, old_right = 0;
    int pad_ready = 0;

    (void)argc;
    (void)argv;

    ps3scale_game_list_init(&g_games);
    load_app_settings();

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

    /* Start the supplied GTA V remix automatically when the app opens. */
    (void)initialize_audio_backend();

    scan_library();

    while (!g_exit_requested) {
        u16 up = 0, down = 0, square = 0, cross = 0, circle = 0;
        u16 l1 = 0, r1 = 0, triangle = 0, select = 0;
        u16 left = 0, right = 0;

        sysUtilCheckCallback();
        g_frame_counter++;

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
                    left = pad_data.BTN_LEFT;
                    right = pad_data.BTN_RIGHT;
                    break;
                }
            }
        }

        if (!g_osk_active) {
            if (g_screen == SCREEN_HOME) {
                if (up && !old_up)
                    g_home_selected = (g_home_selected + 3) % 4;
                if (down && !old_down)
                    g_home_selected = (g_home_selected + 1) % 4;
                if (cross && !old_cross) {
                    if (g_home_selected == 0) {
                        g_screen = SCREEN_SCAN;
                        g_selected = 0;
                    } else if (g_home_selected == 1) {
                        g_screen = SCREEN_SETTINGS;
                    } else if (g_home_selected == 2) {
                        g_screen = SCREEN_TOOLS;
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
            } else if (g_screen == SCREEN_SETTINGS) {
                if (up && !old_up)
                    g_settings_selected = (g_settings_selected + SETTINGS_COUNT - 1) % SETTINGS_COUNT;
                if (down && !old_down)
                    g_settings_selected = (g_settings_selected + 1) % SETTINGS_COUNT;

                if (cross && !old_cross) {
                    if (g_settings_selected == 0) {
                        set_background_music_enabled(!g_music_enabled);
                        (void)save_app_settings();
                    } else if (g_settings_selected == 2) {
                        g_animation_enabled = !g_animation_enabled;
                        (void)save_app_settings();
                    } else if (g_settings_selected == 3) {
                        char update_path[PS3SCALE_PATH_MAX];
                        if (find_usb_update_package(update_path, sizeof(update_path))) {
                            const char *usb = strstr(update_path, "/dev_usb");
                            if (usb)
                                snprintf(g_settings_message, sizeof(g_settings_message),
                                         "UPDATE PKG FOUND ON USB%.3s. INSTALL VIA PACKAGE MANAGER.",
                                         usb + 8);
                            else
                                set_settings_message("UPDATE PKG FOUND. INSTALL VIA PACKAGE MANAGER.");
                        } else {
                            set_settings_message("NO VALID PS3 SCALE UPDATE PKG FOUND ON USB.");
                        }
                    } else if (g_settings_selected == 4) {
                        scan_library();
                        snprintf(g_settings_message, sizeof(g_settings_message),
                                 "SCAN COMPLETE: %u PS3 / %u PS2 TITLES.",
                                 (unsigned)library_total_count(LIBRARY_PS3),
                                 (unsigned)library_total_count(LIBRARY_PS2));
                    } else if (g_settings_selected == 5) {
                        g_music_enabled = 1;
                        g_music_volume = 180;
                        g_animation_enabled = 1;
                        (void)save_app_settings();
                    } else if (g_settings_selected == 6) {
                        int copy_result = copy_music_to_internal();
                        if (copy_result == 0)
                            set_settings_message("MUSIC COPIED TO INTERNAL HDD. RESTART APP, THEN USB CAN BE REMOVED.");
                        else if (copy_result == 1)
                            set_settings_message("MUSIC IS ALREADY ON INTERNAL HDD.");
                        else
                            set_settings_message("COPY FAILED. CHECK USB MUSIC FILE AND HDD SPACE.");
                    }
                }

                if (g_settings_selected == 1) {
                    if (left && !old_left) {
                        set_background_music_volume(g_music_volume - 13);
                        (void)save_app_settings();
                    }
                    if (right && !old_right) {
                        set_background_music_volume(g_music_volume + 13);
                        (void)save_app_settings();
                    }
                }

                if (circle && !old_circle)
                    g_screen = SCREEN_HOME;
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
        old_left = left;
        old_right = right;

        if (!g_osk_active) {
            if (g_screen == SCREEN_SCAN || g_screen == SCREEN_DETAILS)
                update_selected_cover();
            draw_ui();
        }
    }

    if (g_osk_active) {
        (void)oskAbort();
        (void)oskUnloadAsync(&g_osk_result);
        (void)sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT1);
    }
    shutdown_audio_backend();
    sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
    if (g_cover_texture) ya2d_freeTexture(g_cover_texture);
    g_cover_texture = NULL;
    if (pad_ready) ya2d_deinit();
    return 0;
}
