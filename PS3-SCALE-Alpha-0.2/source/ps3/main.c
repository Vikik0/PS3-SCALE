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
#include <ya2d/ya2d.h>
#include <matrix.h>
#include <zlib.h>

#include "ps3scale/game.h"
#include "ps3scale/scanner.h"
#include "ps3scale/ps2_config.h"
#include "ps3scale/locale.h"
#include "ps3scale/hardware_info.h"
#include "font8x12.h"
#include "font_bitmap_z.h"

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
    SCREEN_ABOUT,
    SCREEN_LANGUAGE,
    SCREEN_BOOT,
    SCREEN_KEYBOARD
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
static int g_apollo_installed = 0;
static int g_artemis_installed = 0;
static int g_webman_installed = 0;
static char g_tools_message[96] = "CROSS: REFRESH TOOL STATUS.";
#define SETTINGS_COUNT 8
#define PS3SCALE_SETTINGS_PATH "/dev_hdd0/game/PS3SCALE1/USRDIR/settings.cfg"
static char g_music_path[PS3SCALE_PATH_MAX] = "";
enum {
    LANGUAGE_UNSET = -1,
    LANGUAGE_ENGLISH = PS3SCALE_LANG_ENGLISH,
    LANGUAGE_ARABIC = PS3SCALE_LANG_ARABIC,
    LANGUAGE_SPANISH = PS3SCALE_LANG_SPANISH
};
static int g_language = LANGUAGE_UNSET;
static int g_language_choice = LANGUAGE_ENGLISH;
static int g_keyboard_selected = 0;
static char g_search_backup[64] = "";
static int g_boot_stage = 0;
static int g_boot_stage_frames = 0;
static volatile int g_boot_scan_done = 0;
static int g_boot_scan_thread_started = 0;
static sys_ppu_thread_t g_boot_scan_thread;
static int g_app_files_ok = 0;
static int g_extended_font_ready = 0;
static unsigned char g_extended_font_bitmap[PS3SCALE_FONT_RAW_SIZE];
static ps3scale_hardware_info_t g_hardware_info;
static int g_hardware_info_valid = 0;
static unsigned char g_extended_glyph_min_x[256];
static unsigned char g_extended_glyph_ink_width[256];
static int g_ui_font_width = 14;

static void clamp_selection(void);
static void scan_library(void);

static void app_sysutil_callback(u64 status, u64 param, void *userdata)
{
    (void)param; (void)userdata;
    if (status == SYSUTIL_EXIT_GAME) g_exit_requested = 1;
}

static int open_search_keyboard(void)
{
    snprintf(g_search_backup, sizeof(g_search_backup), "%s", g_search);
    g_keyboard_selected = 0;
    g_screen = SCREEN_KEYBOARD;
    return 0;
}

static int base64_value(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static int decode_base64(const char *source, unsigned char *output,
                         size_t capacity)
{
    uint32_t value = 0;
    int bits = -8;
    size_t used = 0, i;
    for (i = 0; source && source[i]; i++) {
        int digit;
        if (source[i] == '=') break;
        digit = base64_value(source[i]);
        if (digit < 0) continue;
        value = (value << 6) | (uint32_t)digit;
        bits += 6;
        if (bits >= 0) {
            if (used >= capacity) return -1;
            output[used++] = (unsigned char)((value >> bits) & 0xff);
            bits -= 8;
        }
    }
    return (int)used;
}

static void build_extended_font_metrics(void)
{
    const size_t bytes_per_row = (PS3SCALE_FONT_WIDTH + 7u) / 8u;
    const size_t bytes_per_glyph = bytes_per_row * PS3SCALE_FONT_HEIGHT;
    unsigned glyph;

    memset(g_extended_glyph_min_x, 0, sizeof(g_extended_glyph_min_x));
    memset(g_extended_glyph_ink_width, 0, sizeof(g_extended_glyph_ink_width));

    /*
     * The atlas is 1bpp, row-major, and uses BIT7_FIRST_PIXEL. Measure the
     * visible ink bounds once at startup so Arabic glyphs can be positioned
     * by their actual bitmap width instead of a guessed fixed character gap.
     */
    for (glyph = PS3SCALE_FONT_EXTENDED_FIRST;
         glyph <= PS3SCALE_FONT_LAST; glyph++) {
        size_t offset =
            (size_t)(glyph - PS3SCALE_FONT_FIRST) * bytes_per_glyph;
        unsigned min_x = PS3SCALE_FONT_WIDTH;
        unsigned max_x = 0;
        unsigned row, col;
        int found = 0;

        if (offset + bytes_per_glyph > sizeof(g_extended_font_bitmap))
            continue;

        for (row = 0; row < PS3SCALE_FONT_HEIGHT; row++) {
            for (col = 0; col < PS3SCALE_FONT_WIDTH; col++) {
                size_t byte_index = offset +
                    (size_t)row * bytes_per_row + (size_t)(col >> 3);
                unsigned char mask =
                    (unsigned char)(0x80u >> (col & 7u));

                if ((g_extended_font_bitmap[byte_index] & mask) != 0) {
                    if (col < min_x) min_x = col;
                    if (!found || col > max_x) max_x = col;
                    found = 1;
                }
            }
        }

        if (found) {
            g_extended_glyph_min_x[glyph] = (unsigned char)min_x;
            g_extended_glyph_ink_width[glyph] =
                (unsigned char)(max_x - min_x + 1u);
        } else {
            /* Keep a safe full-cell advance for an empty/unmapped glyph. */
            g_extended_glyph_min_x[glyph] = 0;
            g_extended_glyph_ink_width[glyph] = PS3SCALE_FONT_WIDTH;
        }
    }
}

static void set_ui_font_size(int width, int height)
{
    g_ui_font_width = width;
    SetFontSize(width, height);
}

static int load_extended_font(void)
{
    unsigned char compressed[2048];
    int compressed_size;
    uLongf output_size = (uLongf)sizeof(g_extended_font_bitmap);
    u8 *texture_cursor;
    const size_t bytes_per_glyph =
        (PS3SCALE_FONT_WIDTH * PS3SCALE_FONT_HEIGHT) / 8u;
    const size_t extended_offset =
        (size_t)(PS3SCALE_FONT_EXTENDED_FIRST - PS3SCALE_FONT_FIRST) *
        bytes_per_glyph;
    const size_t extended_size =
        (size_t)(PS3SCALE_FONT_LAST - PS3SCALE_FONT_EXTENDED_FIRST + 1u) *
        bytes_per_glyph;

    compressed_size = decode_base64(ps3scale_font_z64, compressed,
                                     sizeof(compressed));
    if (compressed_size <= 0) return -1;
    if (uncompress(g_extended_font_bitmap, &output_size, compressed,
                   (uLong)compressed_size) != Z_OK ||
        output_size != sizeof(g_extended_font_bitmap))
        return -1;
    if (extended_offset + extended_size > sizeof(g_extended_font_bitmap))
        return -1;

    build_extended_font_metrics();

    /*
     * libfont keeps each bitmap range as a separate font. Register the
     * original ASCII bitmap as font 0, then the Arabic/extended atlas as
     * font 1. The compressed atlas starts at codepoint 32, so skip its
     * first 96 ASCII slots before registering codepoints 128..255.
     */
    ResetFont();
    texture_cursor = (u8 *)g_font_texture;
    texture_cursor = AddFontFromBitmapArray((u8 *)ps3scale_font8x12,
        texture_cursor, 32, 126, 8, 12, 1, BIT7_FIRST_PIXEL);
    texture_cursor = AddFontFromBitmapArray(
        g_extended_font_bitmap + extended_offset, texture_cursor,
        PS3SCALE_FONT_EXTENDED_FIRST, PS3SCALE_FONT_LAST,
        PS3SCALE_FONT_WIDTH, PS3SCALE_FONT_HEIGHT, 1, BIT7_FIRST_PIXEL);
    if (!texture_cursor || texture_cursor <= (u8 *)g_font_texture)
        return -1;

    SetCurrentFont(0);
    g_extended_font_ready = 1;
    return 0;
}

static void draw_text(float x, float y, u32 color, const char *text)
{
    char translated[512];
    char glyph_text[512];
    const char *source = text ? text : "";
    size_t i, length;
    int contains_arabic = 0;

    if (g_language == LANGUAGE_ARABIC || g_language == LANGUAGE_SPANISH) {
        (void)ps3scale_locale_translate(g_language, source, translated,
                                        sizeof(translated));
        source = translated;
    }
    if (!g_extended_font_ready &&
        (g_language == LANGUAGE_ARABIC || g_language == LANGUAGE_SPANISH))
        source = text ? text : "";

    (void)ps3scale_locale_to_glyphs(source, glyph_text, sizeof(glyph_text));
    length = strlen(glyph_text);
    for (i = 0; i < length; i++) {
        if (ps3scale_locale_is_arabic_glyph(
                (unsigned char)glyph_text[i])) {
            contains_arabic = 1;
            break;
        }
    }

    SetFontColor(color, 0x00000000);
    if (g_extended_font_ready) {
        float cursor = x;

        for (i = 0; i < length; i++) {
            unsigned char glyph = (unsigned char)glyph_text[i];
            int font_index = ps3scale_locale_glyph_font_index(glyph);

            if (glyph == ' ') {
                cursor += contains_arabic ?
                    ((float)g_ui_font_width * 0.5f) :
                    (float)g_ui_font_width;
                continue;
            }

            SetCurrentFont(font_index);
            if (ps3scale_locale_is_arabic_glyph(glyph)) {
                float scale_x =
                    (float)g_ui_font_width / (float)PS3SCALE_FONT_WIDTH;
                float ink_width =
                    (float)g_extended_glyph_ink_width[glyph];
                unsigned min_x = g_extended_glyph_min_x[glyph];
                unsigned char next_glyph =
                    (i + 1u < length) ?
                    (unsigned char)glyph_text[i + 1u] : 0;

                if (ink_width <= 0.0f)
                    ink_width = (float)PS3SCALE_FONT_WIDTH;

                /* Align visible ink edges, not the empty bitmap-cell edges. */
                DrawChar(cursor - ((float)min_x * scale_x), y, 0.0f, glyph);
                cursor += ink_width * scale_x;

                if (ps3scale_locale_is_arabic_glyph(next_glyph)) {
                    if (ps3scale_locale_glyphs_joined(glyph, next_glyph)) {
                        /* Four source pixels close the joining seam without merging separate glyphs. */
                        cursor -= scale_x * 4.0f;
                    } else {
                        /* Non-joining Arabic letters retain a small natural gap. */
                        cursor += scale_x * 2.0f;
                    }
                } else if (next_glyph != 0 && next_glyph != ' ') {
                    cursor += scale_x;
                }
            } else {
                DrawChar(cursor, y, 0.0f, glyph);
                cursor += (float)g_ui_font_width;
            }
        }
        SetCurrentFont(0);
    } else {
        SetCurrentFont(0);
        DrawString(x, y, glyph_text);
    }
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

static void draw_ambient_ring(float radius, float tilt, float spin,
                               float x, float y, float z, u32 color)
{
    MATRIX rotation, scale, translation, model;
    int i;
    rotation = MatrixMultiply(MatrixRotationY(spin), MatrixRotationX(tilt));
    scale = MatrixScale(radius, radius, radius);
    translation = MatrixTranslation(x, y, z);
    model = MatrixMultiply(MatrixMultiply(scale, rotation), translation);
    tiny3d_SetMatrixModelView(&model);
    tiny3d_SetPolygon(TINY3D_LINE_LOOP);
    for (i = 0; i < 48; i++) {
        float angle = 6.28318530718f * (float)i / 48.0f;
        tiny3d_VertexPos(cosf(angle), sinf(angle), 0.0f);
        if (i == 0) tiny3d_VertexColor(color);
    }
    tiny3d_End();
}

static void draw_ambient_satellite(float orbit_radius, float angle,
                                   float tilt, float spin,
                                   float x, float y, float z, u32 color)
{
    MATRIX rotation, translation, model;
    float px = cosf(angle) * orbit_radius;
    float py = sinf(angle) * orbit_radius;
    rotation = MatrixMultiply(MatrixRotationY(spin), MatrixRotationX(tilt));
    translation = MatrixTranslation(x, y, z);
    model = MatrixMultiply(rotation, translation);
    tiny3d_SetMatrixModelView(&model);
    tiny3d_SetPolygon(TINY3D_LINES);
    tiny3d_VertexPos(px - 0.72f, py, 0.0f);
    tiny3d_VertexColor(color);
    tiny3d_VertexPos(px + 0.72f, py, 0.0f);
    tiny3d_VertexPos(px, py - 0.72f, 0.0f);
    tiny3d_VertexPos(px, py + 0.72f, 0.0f);
    tiny3d_VertexPos(px, py, -0.72f);
    tiny3d_VertexPos(px, py, 0.72f);
    tiny3d_End();
}

static void draw_home_ambient(void)
{
    /* Aurora Core: calm wireframe globe, orbital rings, and drifting particles. */
    const float phase = (float)g_frame_counter * 0.0012f;
    const float bob = sinf(phase * 0.58f) * 2.6f;
    const float center_x = 28.0f;
    const float center_y = 1.5f + bob;
    const float center_z = 82.0f;
    MATRIX projection, rotation, scale, translation, model;
    int latitude, longitude, segment, i;

    tiny3d_Project3D();
    projection = MatrixProjPerspective(70.0f, 16.0f / 9.0f, 0.1f, 300.0f);
    tiny3d_SetProjectionMatrix(&projection);
    tiny3d_SetLightsOff();

    rotation = MatrixMultiply(MatrixRotationY(phase),
                              MatrixRotationX(phase * 0.37f));
    scale = MatrixScale(13.0f, 13.0f, 13.0f);
    translation = MatrixTranslation(center_x, center_y, center_z);
    model = MatrixMultiply(MatrixMultiply(scale, rotation), translation);
    tiny3d_SetMatrixModelView(&model);

    for (latitude = -3; latitude <= 3; latitude++) {
        float lat = (float)latitude * 0.34f;
        float ring_radius = cosf(lat);
        float ring_y = sinf(lat);
        tiny3d_SetPolygon(TINY3D_LINE_LOOP);
        for (segment = 0; segment < 40; segment++) {
            float angle = 6.28318530718f * (float)segment / 40.0f;
            tiny3d_VertexPos(ring_radius * cosf(angle), ring_y,
                             ring_radius * sinf(angle));
            if (segment == 0) {
                u32 color = (latitude == 0) ? 0x65c8e6d0 :
                            ((latitude & 1) ? 0x397fb6a0 : 0x4a9fbe90);
                tiny3d_VertexColor(color);
            }
        }
        tiny3d_End();
    }

    for (longitude = 0; longitude < 10; longitude++) {
        float lon = 6.28318530718f * (float)longitude / 10.0f;
        tiny3d_SetPolygon(TINY3D_LINE_STRIP);
        for (segment = 0; segment <= 18; segment++) {
            float lat = -1.25f + 2.5f * (float)segment / 18.0f;
            float ring_radius = cosf(lat);
            tiny3d_VertexPos(ring_radius * cosf(lon), sinf(lat),
                             ring_radius * sinf(lon));
            if (segment == 0)
                tiny3d_VertexColor(0x397fb6a0);
        }
        tiny3d_End();
    }

    draw_ambient_ring(18.8f, 1.08f, phase * 0.42f,
                      center_x, center_y, center_z, 0x55b9d6b0);
    draw_ambient_ring(21.2f, 0.38f, -phase * 0.28f,
                      center_x, center_y, center_z, 0x397fb690);
    draw_ambient_ring(16.6f, 1.48f, phase * 0.20f,
                      center_x, center_y, center_z, 0x8bd45090);
    draw_ambient_satellite(18.8f, phase * 0.42f + 0.7f,
                           1.08f, phase * 0.42f,
                           center_x, center_y, center_z, 0x8bd450ff);
    draw_ambient_satellite(21.2f, -phase * 0.28f + 2.4f,
                           0.38f, -phase * 0.28f,
                           center_x, center_y, center_z, 0x65c8e6ff);

    tiny3d_Project2D();
    tiny3d_SetMatrixModelView(NULL);
    for (i = 0; i < 16; i++) {
        float px = 492.0f +
            (float)((i * 47 + (int)(g_frame_counter / 4u)) % 330);
        float py = 34.0f + (float)((i * 31) % 430) +
                   sinf(phase * 0.8f + (float)i * 0.71f) * 3.5f;
        float size = (i % 4 == 0) ? 2.0f : 1.0f;
        u32 color = (i % 3 == 0) ? 0x65c8e680 : 0x8bd45060;
        draw_solid_rect(px, py, size, size, color);
    }
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
    g_language = LANGUAGE_UNSET;
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "music=%d", &value) == 1)
            g_music_enabled = value ? 1 : 0;
        else if (sscanf(line, "volume=%d", &value) == 1)
            set_background_music_volume(value);
        else if (sscanf(line, "animation=%d", &value) == 1)
            g_animation_enabled = value ? 1 : 0;
        else if (sscanf(line, "language=%d", &value) == 1 &&
                 value >= LANGUAGE_ENGLISH && value <= LANGUAGE_SPANISH)
            g_language = value;
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
    fprintf(fp, "language=%d\n", g_language);
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


static int file_exists_readonly(const char *path)
{
    FILE *fp;
    if (!path || !path[0]) return 0;
    fp = fopen(path, "rb");
    if (!fp) return 0;
    fclose(fp);
    return 1;
}

static int file_contains_text_readonly(const char *path, const char *needle)
{
    FILE *fp;
    char line[256];
    int found = 0;
    if (!path || !needle || !needle[0]) return 0;
    fp = fopen(path, "r");
    if (!fp) return 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, needle)) {
            found = 1;
            break;
        }
    }
    fclose(fp);
    return found;
}

static void refresh_companion_tool_status(void)
{
    g_apollo_installed = file_exists_readonly(
        "/dev_hdd0/game/NP0APOLLO/USRDIR/EBOOT.BIN");
    g_artemis_installed = file_exists_readonly(
        "/dev_hdd0/game/ARTPS3001/USRDIR/EBOOT.BIN");
    g_webman_installed =
        file_exists_readonly("/dev_hdd0/plugins/webftp_server.sprx") ||
        file_contains_text_readonly("/dev_hdd0/boot_plugins.txt",
                                    "webftp_server.sprx");
    snprintf(g_tools_message, sizeof(g_tools_message),
             "STATUS UPDATED. OPEN INSTALLED TOOLS FROM XMB.");
}

static void draw_language_selection(void)
{
    int i;
    set_ui_font_size(15, 23);
    draw_text(34, 28, 0xcaff39ff, "PS3 SCALE");
    draw_text(34, 88, 0xffffffff, "CHOOSE LANGUAGE");
    draw_text(34, 126, 0xdce3e8ff, "اختر اللغة");
    draw_text(34, 164, 0xdce3e8ff, "Elige el idioma");
    for (i = 0; i < 3; i++) {
        float y = 226.0f + (float)i * 58.0f;
        int selected = (i == g_language_choice);
        if (selected) draw_solid_rect(80.0f, y - 5.0f, 650.0f, 44.0f, 0x173a35ff);
        draw_outline_rect(80.0f, y - 5.0f, 650.0f, 44.0f,
                          selected ? 0x8bd450ff : 0x26445cff);
        draw_text(112, y + 7.0f,
                  selected ? 0xcaff39ff : 0xdce3e8ff,
                  ps3scale_locale_language_name(i));
    }
    draw_text(34, 478, 0x9aa6adff, "UP/DOWN: SELECT  |  CROSS: CONTINUE");
    set_ui_font_size(14, 22);
}

static void draw_boot(void)
{
    const char *stage_text[] = {
        "INITIALIZING GRAPHICS AND CONTROLLER",
        "CHECKING SETTINGS AND AUDIO",
        "SCANNING LOCAL GAME LIBRARY",
        "CHECKING COMPANION TOOLS",
        "READY"
    };
    const int progress[] = {12, 28, 58, 88, 100};
    float width = (640.0f * (float)progress[g_boot_stage]) / 100.0f;
    set_ui_font_size(14, 21);

    draw_text(34, 30, 0xcaff39ff, "PS3 SCALE");
    draw_text(34, 92, 0xffffffff, "STARTING PS3 SCALE");
    draw_text(100, 214, 0xdce3e8ff, stage_text[g_boot_stage]);
    draw_outline_rect(100.0f, 286.0f, 648.0f, 20.0f, 0x42647aff);
    draw_solid_rect(104.0f, 290.0f, width, 12.0f, 0x4aafcaff);
    if (g_boot_stage == 2) {
        float moving = 104.0f + (float)((g_frame_counter * 5u) % 530u);
        draw_solid_rect(moving, 290.0f, 78.0f, 12.0f, 0xcaff39ff);
    }
    if (g_boot_stage < 4)
        draw_text(100, 336, 0x9aa6adff, "PLEASE WAIT...");
    else
        draw_text(100, 336, 0x9ee56aff, "READY");
    draw_text(100, 380, g_app_files_ok ? 0x9ee56aff : 0xffcc66ff,
              g_app_files_ok ? "APP FILE CHECK: OK" : "APP FILE CHECK: VERIFY INSTALL");
    draw_text(34, 478, 0x6f7b83ff,
              "LOCAL CHECKS ONLY  |  NO GAME FILES ARE MODIFIED");
    set_ui_font_size(14, 22);
}

static void draw_keyboard(void)
{
    int i;
    set_ui_font_size(12, 18);
    draw_text(34, 22, 0xcaff39ff, "PS3 SCALE");
    draw_text(34, 56, 0xffffffff, "SEARCH GAMES");
    draw_text(34, 88, 0x9aa6adff, "TYPE TO FILTER THE LIBRARY");
    draw_solid_rect(40.0f, 112.0f, 768.0f, 34.0f, 0x0d1722ff);
    draw_outline_rect(40.0f, 112.0f, 768.0f, 34.0f, 0x42647aff);
    draw_text(54, 120, 0xcaff39ff, g_search[0] ? g_search : " ");
    for (i = 0; i < 40; i++) {
        int row = i / 10;
        int col = i % 10;
        float x = 48.0f + (float)col * 76.0f;
        float y = 160.0f + (float)row * 46.0f;
        int selected = (i == g_keyboard_selected);
        draw_solid_rect(x, y, 68.0f, 38.0f,
                        selected ? 0x1c594dff : 0x111e2bff);
        draw_outline_rect(x, y, 68.0f, 38.0f,
                          selected ? 0x8bd450ff : 0x344f63ff);
        draw_text(x + 23.0f, y + 9.0f, selected ? 0xcaff39ff : 0xdce3e8ff,
                  ps3scale_locale_keyboard_key(g_language, i));
    }
    {
        static const char *actions[5] = {"SPACE","BACKSPACE","CLEAR","APPLY","CANCEL"};
        for (i = 0; i < 5; i++) {
            float x = 36.0f + (float)i * 156.0f;
            int selected = (g_keyboard_selected == 40 + i);
            draw_solid_rect(x, 360.0f, 144.0f, 42.0f,
                            selected ? 0x1c594dff : 0x111e2bff);
            draw_outline_rect(x, 360.0f, 144.0f, 42.0f,
                              selected ? 0x8bd450ff : 0x344f63ff);
            draw_text(x + 14.0f, 372.0f,
                      selected ? 0xcaff39ff : 0xdce3e8ff, actions[i]);
        }
    }
    draw_text(34, 440, 0x9aa6adff,
              "ARROWS MOVE  |  CROSS SELECT  |  CIRCLE CANCEL");
    draw_text(34, 478, 0x6f7b83ff,
              "CUSTOM KEYBOARD  |  NO PS3 SYSTEM KEYBOARD REQUIRED");
    set_ui_font_size(14, 22);
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
    set_ui_font_size(12, 18);

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
    set_ui_font_size(14, 22);
}

static void draw_scan(void)
{
    char line[256];
    size_t i, count, start;
    size_t ps3_count = library_total_count(LIBRARY_PS3);
    size_t ps2_count = library_total_count(LIBRARY_PS2);
    const ps3scale_game_t *selected;

    /* Keep the scanner readable on 720p TVs: two fixed, non-overlapping panels. */
    set_ui_font_size(9, 14);
    clamp_selection();
    count = library_count(g_library);
    selected = selected_game();

    draw_text(28, 16, 0xcaff39ff, "PS3 SCALE");
    draw_text(178, 16, 0x9aa6adff, "SCAN GAMES");

    draw_text(28, 48, 0xffffffff, "PLATFORM:");
    draw_text(140, 48, g_library == LIBRARY_PS3 ? 0xcaff39ff : 0x9aa6adff, "L1  PS3");
    draw_text(250, 48, g_library == LIBRARY_PS2 ? 0xcaff39ff : 0x9aa6adff, "R1  PS2");
    draw_text(370, 48, 0x9aa6adff, "SELECT/START: SEARCH");

    draw_solid_rect(24.0f, 72.0f, 798.0f, 25.0f, 0x0d1722ff);
    snprintf(line, sizeof(line), "SEARCH: %.46s",
             g_search[0] ? g_search : "ALL GAMES");
    draw_text(32, 78, 0xdce3e8ff, line);
    snprintf(line, sizeof(line), "PS3: %u  |  PS2: %u  |  RESULTS: %u",
             (unsigned)ps3_count, (unsigned)ps2_count, (unsigned)count);
    draw_text(28, 108, 0x9aa6adff, line);

    draw_outline_rect(22.0f, 138.0f, 402.0f, 292.0f, 0x26445cff);
    draw_outline_rect(440.0f, 138.0f, 382.0f, 292.0f, 0x26445cff);
    draw_text(32, 148, 0xffffffff,
              g_library == LIBRARY_PS3 ? "PS3 GAME LIST" : "PS2 GAME LIST");
    draw_text(452, 148, 0xffffffff, "SELECTED GAME");

    start = (g_selected > 8) ? (size_t)(g_selected - 8) : 0;
    for (i = 0; i < 9 && start + i < count; i++) {
        const ps3scale_game_t *game = library_game_at(start + i);
        int is_selected = ((int)(start + i) == g_selected);
        const char *kind;
        float row_y = 178.0f + (float)i * 27.0f;
        if (!game) continue;
        if (game->kind == PS3SCALE_GAME_PS3_ISO ||
            game->kind == PS3SCALE_GAME_PS2_ISO)
            kind = "ISO";
        else if (game->kind == PS3SCALE_GAME_PS2_CLASSIC)
            kind = "PS2C";
        else if (game->kind == PS3SCALE_GAME_PS3_HDD ||
                 game->kind == PS3SCALE_GAME_PS2_HDD)
            kind = "HDD";
        else
            kind = "DIR";
        if (is_selected)
            draw_solid_rect(27.0f, row_y - 2.0f, 390.0f, 22.0f, 0x14261dff);
        snprintf(line, sizeof(line), "%c %-4s %.27s",
                 is_selected ? '>' : ' ', kind, game->title);
        draw_text(32, row_y, is_selected ? 0xcaff39ff : 0xdce3e8ff, line);
    }

    if (selected) {
        snprintf(line, sizeof(line), "TITLE: %.21s", selected->title);
        draw_text(452, 178, 0xdce3e8ff, line);
        snprintf(line, sizeof(line), "TYPE: %.22s",
                 ps3scale_game_kind_name(selected->kind));
        draw_text(452, 202, 0xdce3e8ff, line);
        if (g_library == LIBRARY_PS3) {
            snprintf(line, sizeof(line), "TITLE ID: %.16s",
                     selected->title_id[0] ? selected->title_id : "NOT READ");
        } else {
            snprintf(line, sizeof(line), "PS2 SERIAL: %.16s",
                     selected->disc_id[0] ? selected->disc_id :
                     (selected->title_id[0] ? selected->title_id : "NOT READ"));
        }
        draw_text(452, 226, 0xdce3e8ff, line);
        snprintf(line, sizeof(line), "PATH: %.20s", selected->path);
        draw_text(452, 250, 0x9aa6adff, line);
        draw_text(452, 274, 0xcaff39ff, "CROSS: OPEN DETAILS");

        /* Cover art is isolated on the far right, below the metadata. */
        if (g_cover_texture)
            ya2d_drawTextureEx(g_cover_texture, 680.0f, 282.0f,
                               0.5f, 112.0f, 142.0f);
        else {
            draw_solid_rect(680.0f, 282.0f, 112.0f, 142.0f, 0x101b27ff);
            draw_outline_rect(680.0f, 282.0f, 112.0f, 142.0f, 0x344654ff);
            draw_text(690, 346, 0x6f7b83ff, "NO COVER");
        }
    } else {
        draw_text(452, 184, 0x9aa6adff, "NO MATCHING GAMES FOUND.");
        draw_text(452, 210, 0x9aa6adff, "PRESS SELECT OR START TO SEARCH.");
    }

    if (g_config_preview && selected &&
        selected->kind == PS3SCALE_GAME_PS2_ISO) {
        uint8_t config_data[32];
        size_t config_size = 0;
        if (ps3scale_ps2_config_build(config_data, sizeof(config_data),
                PS3SCALE_PS2_CFG_DISABLE_SMOOTHING |
                PS3SCALE_PS2_CFG_ENABLE_L2H, &config_size) == 0) {
            snprintf(line, sizeof(line), "PS2 CONFIG PREVIEW: %u BYTES",
                     (unsigned)config_size);
            draw_text(452, 438, 0xcaff39ff, line);
        }
    }
    if (g_games.truncated)
        draw_text(28, 438, 0xffcc66ff,
                  "WARNING: LIBRARY LIMIT REACHED (256 TITLES)");

    draw_text(28, 470, 0x9aa6adff,
              "UP/DOWN MOVE  |  L1/R1 PLATFORM  |  CROSS DETAILS  |  SELECT/START SEARCH");
    draw_text(28, 492, 0x6f7b83ff,
              "SQUARE RESCAN  |  TRIANGLE PS2 PREVIEW  |  CIRCLE BACK");
    set_ui_font_size(14, 22);
}

static void draw_details(void)
{
    char line[256];
    const ps3scale_game_t *game = selected_game();
    set_ui_font_size(11, 17);
    draw_text(28, 22, 0xcaff39ff, "PS3 SCALE");
    draw_text(180, 25, 0x9aa6adff, "GAME DETAILS");
    if (!game) {
        draw_text(34, 120, 0x9aa6adff, "NO GAME SELECTED.");
        draw_text(34, 478, 0x9aa6adff, "CIRCLE: BACK");
        set_ui_font_size(14, 22);
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
    set_ui_font_size(14, 22);
}

static void draw_settings(void)
{
    static char *labels[SETTINGS_COUNT] = {
        "BACKGROUND MUSIC", "MUSIC VOLUME", "HOME ANIMATION",
        "CHECK FOR USB UPDATE", "RESCAN GAME LIBRARY",
        "RESTORE DEFAULT SETTINGS", "COPY MUSIC TO INTERNAL HDD",
        "LANGUAGE"
    };
    char value[96];
    int i;
    set_ui_font_size(12, 18);
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
        else if (i == 6) snprintf(value, sizeof(value), "IMPORT FROM USB");
        else snprintf(value, sizeof(value), "%s",
                      ps3scale_locale_language_name(g_language));
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
    set_ui_font_size(14, 22);
}

static void draw_tools(void)
{
    char line[128];
    set_ui_font_size(10, 15);
    draw_text(34, 24, 0xcaff39ff, "PS3 SCALE");
    draw_text(34, 62, 0xffffffff, "COMPANION TOOLS");

    draw_text(34, 112, 0xcaff39ff, "APOLLO SAVE TOOL");
    snprintf(line, sizeof(line), "STATUS: %s",
             g_apollo_installed ? "INSTALLED" : "NOT DETECTED");
    draw_text(610, 112, g_apollo_installed ? 0x9ee56aff : 0xffcc66ff, line);
    draw_text(34, 140, 0xdce3e8ff,
              "SAVE BACKUP / RESTORE / RESIGN / TROPHY AND LICENSE TOOLS.");

    draw_text(34, 190, 0xcaff39ff, "ARTEMIS PS3");
    snprintf(line, sizeof(line), "STATUS: %s",
             g_artemis_installed ? "INSTALLED" : "NOT DETECTED");
    draw_text(610, 190, g_artemis_installed ? 0x9ee56aff : 0xffcc66ff, line);
    draw_text(34, 218, 0xdce3e8ff,
              "TITLE-SPECIFIC CHEAT DATABASE AND ITS OWN RUNTIME PLUGIN.");

    draw_text(34, 268, 0xcaff39ff, "webMAN MOD");
    snprintf(line, sizeof(line), "STATUS: %s",
             g_webman_installed ? "PLUGIN FOUND" : "NOT DETECTED");
    draw_text(610, 268, g_webman_installed ? 0x9ee56aff : 0xffcc66ff, line);
    draw_text(34, 296, 0xdce3e8ff,
              "GAME MOUNTING / FTP / TEMPERATURE / OPTIONAL PS3MAPI.");

    draw_text(34, 354, 0xffffffff, "SAFE INTEGRATION");
    draw_text(34, 382, 0x9aa6adff,
              "PS3 SCALE DOES NOT REPLACE OR MODIFY THESE APPLICATIONS.");
    draw_text(34, 410, 0x9aa6adff,
              "OPEN INSTALLED TOOLS FROM THE XMB GAME MENU.");
    draw_text(34, 446, 0x9aa6adff, g_tools_message);
    draw_text(34, 478, 0x9aa6adff,
              "CROSS: REFRESH TOOL STATUS  |  CIRCLE: BACK");
    set_ui_font_size(14, 22);
}

static void draw_about(void)
{
    set_ui_font_size(11, 17);
    draw_text(34, 24, 0xcaff39ff, "PS3 SCALE");
    draw_text(34, 62, 0xffffffff, "ABOUT");
    draw_text(34, 112, 0xdce3e8ff,
              "PS3 / PS2 GAME LIBRARY AND GRAPHICS PROFILE MANAGER.");
    draw_text(34, 152, 0xdce3e8ff,
              "TARGET HARDWARE: PS3 FAT / SLIM / SUPER SLIM.");
    draw_text(34, 192, 0x9aa6adff,
              "ALPHA 0.2 - EXPERIMENTAL HOMEBREW SOFTWARE.");
    draw_text(34, 232, 0x9aa6adff,
              "SCANNING IS READ-ONLY. ORIGINAL GAME FILES ARE PRESERVED.");
    draw_text(34, 272, 0x9aa6adff,
              "NO UNIVERSAL UPSCALER OR FRAME GENERATION IS CLAIMED.");
    draw_text(34, 312, 0x9aa6adff,
              "PROFILES NEED EXACT TITLE / UPDATE MATCHING.");

    draw_text(34, 352, 0xffffffff, "HARDWARE INFORMATION");
    if (g_hardware_info_valid) {
        draw_text(34, 382, 0xdce3e8ff, "DETECTED MODEL:");
        draw_text(210, 382, 0xdce3e8ff, g_hardware_info.model_series);
        draw_text(34, 416, 0xdce3e8ff, "CONSOLE TYPE:");
        draw_text(170, 416, 0xdce3e8ff,
                  ps3scale_console_type_name(g_hardware_info.console_type));
        draw_text(300, 416, 0xdce3e8ff, "FIRMWARE:");
        draw_text(404, 416, 0xdce3e8ff, g_hardware_info.firmware_version);
        draw_text(34, 448, 0xdce3e8ff, "PLATFORM ID:");
        draw_text(170, 448, 0xdce3e8ff, g_hardware_info.platform_id);
    } else {
        draw_text(34, 382, 0xffcc66ff, "HARDWARE INFO UNAVAILABLE.");
    }
    draw_text(34, 478, 0x9aa6adff, "CIRCLE: BACK");
    set_ui_font_size(14, 22);
}

static void draw_ui(void)
{
    tiny3d_Clear(0xff070a0d, TINY3D_CLEAR_ALL);
    tiny3d_Project2D();
    /* Keep the same calm 3D ambience behind every app page. */
    if (g_animation_enabled)
        draw_home_ambient();
    switch (g_screen) {
        case SCREEN_LANGUAGE: draw_language_selection(); break;
        case SCREEN_BOOT: draw_boot(); break;
        case SCREEN_KEYBOARD: draw_keyboard(); break;
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

static void boot_scan_worker(void *arg)
{
    (void)arg;
    scan_library();
    g_boot_scan_done = 1;
}

static void start_boot_scan(void)
{
    int rc;
    g_boot_scan_done = 0;
    rc = sysThreadCreate(&g_boot_scan_thread, boot_scan_worker, NULL,
                         1500, 0x8000, THREAD_JOINABLE,
                         "PS3 SCALE Library Scan");
    if (rc == 0) {
        g_boot_scan_thread_started = 1;
    } else {
        scan_library();
        g_boot_scan_done = 1;
    }
}

static void boot_tick(void)
{
    if (g_boot_stage == 0) {
        if (++g_boot_stage_frames >= 22) {
            g_boot_stage = 1;
            g_boot_stage_frames = 0;
        }
    } else if (g_boot_stage == 1) {
        if (++g_boot_stage_frames >= 22) {
            refresh_companion_tool_status();
            g_boot_stage = 2;
            g_boot_stage_frames = 0;
            start_boot_scan();
        }
    } else if (g_boot_stage == 2) {
        if (g_boot_scan_done) {
            u64 thread_result = 0;
            if (g_boot_scan_thread_started) {
                (void)sysThreadJoin(g_boot_scan_thread, &thread_result);
                g_boot_scan_thread_started = 0;
            }
            g_boot_stage = 3;
            g_boot_stage_frames = 0;
        }
    } else if (g_boot_stage == 3) {
        if (++g_boot_stage_frames >= 24) {
            g_boot_stage = 4;
            g_boot_stage_frames = 0;
        }
    } else if (g_boot_stage == 4) {
        if (++g_boot_stage_frames >= 36) {
            g_screen = SCREEN_HOME;
            g_boot_stage_frames = 0;
        }
    }
}

static void keyboard_move(int direction)
{
    if (g_keyboard_selected >= 40) {
        int col = g_keyboard_selected - 40;
        if (direction == 0) g_keyboard_selected = 30 + col * 2;
        else if (direction == 1) g_keyboard_selected = col;
        else if (direction == 2) g_keyboard_selected = 40 + ((col + 1) % 5);
        else g_keyboard_selected = 40 + ((col + 4) % 5);
        return;
    }
    {
        int row = g_keyboard_selected / 10;
        int col = g_keyboard_selected % 10;
        if (direction == 0)
            g_keyboard_selected = row > 0 ? g_keyboard_selected - 10 :
                                  40 + (col / 2);
        else if (direction == 1)
            g_keyboard_selected = row < 3 ? g_keyboard_selected + 10 :
                                  40 + (col / 2);
        else if (direction == 2)
            g_keyboard_selected = row * 10 + ((col + 1) % 10);
        else
            g_keyboard_selected = row * 10 + ((col + 9) % 10);
    }
}

static void keyboard_append(const char *value)
{
    size_t used, amount;
    if (!value) return;
    used = strlen(g_search);
    amount = strlen(value);
    if (used + amount < sizeof(g_search)) {
        memcpy(g_search + used, value, amount + 1);
        g_selected = 0;
        clamp_selection();
    }
}

static void keyboard_backspace(void)
{
    size_t n = strlen(g_search);
    if (n == 0) return;
    n--;
    while (n > 0 && (((unsigned char)g_search[n] & 0xc0) == 0x80)) n--;
    g_search[n] = '\0';
    g_selected = 0;
    clamp_selection();
}

static void keyboard_cancel(void)
{
    snprintf(g_search, sizeof(g_search), "%s", g_search_backup);
    g_selected = 0;
    g_screen = SCREEN_SCAN;
    clamp_selection();
}

static void keyboard_apply(void)
{
    g_selected = 0;
    g_config_preview = 0;
    g_screen = SCREEN_SCAN;
    clamp_selection();
}

static void keyboard_press(void)
{
    if (g_keyboard_selected < 40) {
        keyboard_append(ps3scale_locale_keyboard_key(g_language,
                                                     g_keyboard_selected));
    } else if (g_keyboard_selected == 40) {
        keyboard_append(" ");
    } else if (g_keyboard_selected == 41) {
        keyboard_backspace();
    } else if (g_keyboard_selected == 42) {
        g_search[0] = '\0';
        g_selected = 0;
    } else if (g_keyboard_selected == 43) {
        keyboard_apply();
    } else {
        keyboard_cancel();
    }
}

int main(int argc, char *argv[])
{
    padInfo pad_info;
    padData pad_data;
    u16 old_up = 0, old_down = 0, old_square = 0;
    u16 old_cross = 0, old_circle = 0, old_l1 = 0, old_r1 = 0;
    u16 old_triangle = 0, old_select = 0, old_start = 0;
    u16 old_left = 0, old_right = 0;
    int pad_ready = 0;

    (void)argc;
    (void)argv;

    ps3scale_game_list_init(&g_games);
    load_app_settings();
    g_hardware_info_valid =
        (ps3scale_hardware_detect(&g_hardware_info) == 0);

    if (!ya2d_init()) return 1;
    pad_ready = 1;
    g_font_texture = tiny3d_AllocTexture(2 * 1024 * 1024);
    if (!g_font_texture) return 1;

    ResetFont();
    if (load_extended_font() != 0) {
        ResetFont();
        AddFontFromBitmapArray((u8 *)ps3scale_font8x12,
                               (u8 *)g_font_texture,
                               32, 126, 8, 12, 1, BIT7_FIRST_PIXEL);
        g_extended_font_ready = 0;
    }
    SetCurrentFont(0);
    set_ui_font_size(14, 22);

    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, app_sysutil_callback, NULL);
    g_app_files_ok =
        file_exists_readonly("/dev_hdd0/game/PS3SCALE1/USRDIR/EBOOT.BIN") ||
        file_exists_readonly("/dev_hdd0/game/PS3SCALE1/PARAM.SFO");

    /* Start the supplied theme as soon as the application is initialized. */
    (void)initialize_audio_backend();

    if (g_language == LANGUAGE_UNSET) {
        g_language_choice = LANGUAGE_ENGLISH;
        g_screen = SCREEN_LANGUAGE;
    } else {
        g_language_choice = g_language;
        g_screen = SCREEN_BOOT;
    }
    g_boot_stage = 0;
    g_boot_stage_frames = 0;

    while (!g_exit_requested) {
        u16 up = 0, down = 0, square = 0, cross = 0, circle = 0;
        u16 l1 = 0, r1 = 0, triangle = 0, select = 0;
        u16 left = 0, right = 0, start = 0;

        sysUtilCheckCallback();
        g_frame_counter++;

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
                    triangle = pad_data.BTN_TRIANGLE;
                    select = pad_data.BTN_SELECT;
                    start = pad_data.BTN_START;
                    left = pad_data.BTN_LEFT;
                    right = pad_data.BTN_RIGHT;
                    break;
                }
            }
        }

        if (g_screen == SCREEN_LANGUAGE) {
            if (up && !old_up)
                g_language_choice = (g_language_choice + 2) % 3;
            if (down && !old_down)
                g_language_choice = (g_language_choice + 1) % 3;
            if (cross && !old_cross) {
                g_language = g_language_choice;
                (void)save_app_settings();
                g_boot_stage = 0;
                g_boot_stage_frames = 0;
                g_screen = SCREEN_BOOT;
            }
        } else if (g_screen == SCREEN_BOOT) {
            boot_tick();
        } else if (g_screen == SCREEN_KEYBOARD) {
            if (up && !old_up) keyboard_move(0);
            if (down && !old_down) keyboard_move(1);
            if (right && !old_right) keyboard_move(2);
            if (left && !old_left) keyboard_move(3);
            if (cross && !old_cross) keyboard_press();
            if (circle && !old_circle) keyboard_cancel();
        } else if (g_screen == SCREEN_HOME) {
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
                    refresh_companion_tool_status();
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
            if ((select && !old_select) || (start && !old_start))
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
                        set_settings_message("NO PS3 SCALE UPDATE PACKAGE FOUND ON USB.");
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
                    if (copy_result == 0) {
                        shutdown_audio_backend();
                        if (initialize_audio_backend() == 0)
                            set_settings_message("MUSIC COPIED; PLAYBACK NOW USES INTERNAL HDD.");
                        else
                            set_settings_message("MUSIC COPIED TO HDD. RESTART APP TO START AUDIO.");
                    } else if (copy_result == 1) {
                        shutdown_audio_backend();
                        if (initialize_audio_backend() == 0)
                            set_settings_message("MUSIC ALREADY ON HDD; PLAYBACK SWITCHED TO INTERNAL.");
                        else
                            set_settings_message("MUSIC ALREADY ON HDD. RESTART APP IF AUDIO IS STOPPED.");
                    } else {
                        set_settings_message("COPY FAILED. CHECK USB MUSIC FILE AND HDD SPACE.");
                    }
                } else if (g_settings_selected == 7) {
                    g_language = (g_language + 1) % 3;
                    g_language_choice = g_language;
                    (void)save_app_settings();
                }
            }

            if (g_settings_selected == 7) {
                if (left && !old_left) {
                    g_language = (g_language + 2) % 3;
                    g_language_choice = g_language;
                    (void)save_app_settings();
                }
                if (right && !old_right) {
                    g_language = (g_language + 1) % 3;
                    g_language_choice = g_language;
                    (void)save_app_settings();
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
        } else if (g_screen == SCREEN_TOOLS) {
            if (cross && !old_cross)
                refresh_companion_tool_status();
            if (circle && !old_circle)
                g_screen = SCREEN_HOME;
        } else if (circle && !old_circle) {
            g_screen = SCREEN_HOME;
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
        old_start = start;
        old_left = left;
        old_right = right;

        if (g_screen == SCREEN_SCAN || g_screen == SCREEN_DETAILS)
            update_selected_cover();
        draw_ui();
    }

    if (g_boot_scan_thread_started) {
        u64 thread_result = 0;
        (void)sysThreadJoin(g_boot_scan_thread, &thread_result);
        g_boot_scan_thread_started = 0;
    }
    shutdown_audio_backend();
    sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
    if (g_cover_texture) ya2d_freeTexture(g_cover_texture);
    g_cover_texture = NULL;
    if (pad_ready) ya2d_deinit();
    return 0;
}
