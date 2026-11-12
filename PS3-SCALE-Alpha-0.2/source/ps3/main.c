#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
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
#include "ps3scale/webman.h"
#include "ps3scale/ps2_config.h"
#include "ps3scale/ps2_config_db.h"
#include "ps3scale/locale.h"
#include "ps3scale/hardware_info.h"
#include "ps3scale/catalog.h"
#include "ps3scale/community.h"
#include "ps3scale/online.h"
#include "ps3scale/profile_apply.h"
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
    SCREEN_ONLINE_DB,
    SCREEN_SETTINGS,
    SCREEN_TOOLS,
    SCREEN_ABOUT,
    SCREEN_HELP,
    SCREEN_SUPPORT_FORM,
    SCREEN_SPLASH,
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
static int g_help_selected = 0;
static int g_support_selected = 0;
static int g_support_is_feedback = 0;
static int g_keyboard_support_mode = 0;
static char g_support_title[64] = "";
static char g_support_details[64] = "";
static char g_support_status[128] = "LOCAL OUTBOX READY.";
static sys_ppu_thread_t g_support_thread;
static volatile int g_support_send_running = 0;
static volatile int g_support_send_done = 0;
static volatile int g_support_send_result = -1;
static int g_support_thread_started = 0;
static char g_support_json[1024];
static app_screen_t g_screen = SCREEN_HOME;
static app_screen_t g_splash_next_screen = SCREEN_BOOT;
static int g_splash_frames = 0;
static ps3scale_ps2_config_db_t g_ps2_config_db;
static sys_ppu_thread_t g_ps2_config_download_thread;
static int g_ps2_config_download_thread_started = 0;
static volatile int g_ps2_config_download_running = 0;
static volatile int g_ps2_config_download_done = 0;
static volatile int g_ps2_config_download_result = 0;
static char g_ps2_config_download_url[512];
static char g_ps2_config_download_path[PS3SCALE_PATH_MAX];
static char g_ps2_config_download_temp_path[PS3SCALE_PATH_MAX];
static char g_ps2_config_download_type[PS3SCALE_PS2_CONFIG_TYPE_MAX];
static char g_search[64] = "";
static u32 *g_font_texture = NULL;
static ya2d_Texture *g_cover_texture = NULL;
static ya2d_Texture *g_apollo_texture = NULL;
static char g_cover_path[PS3SCALE_PATH_MAX] = "";

#define PS3SCALE_MUSIC_PATH "/dev_hdd0/game/PS3SCALE1/USRDIR/theme.mp3"
#define PS3SCALE_MUSIC1_PATH "/dev_hdd0/game/PS3SCALE1/USRDIR/music1.mp3"
#define PS3SCALE_MUSIC2_PATH PS3SCALE_MUSIC_PATH
#define PS3SCALE_APOLLO_LOGO_PATH "/dev_hdd0/game/PS3SCALE1/USRDIR/apollo-save.png"
#define PS3SCALE_SUPPORT_DIR "/dev_hdd0/game/PS3SCALE1/USRDIR/support"
#define PS3SCALE_SUPPORT_OUTBOX_PATH "/dev_hdd0/game/PS3SCALE1/USRDIR/support/outbox.txt"
/* Replace with the URL of the separate PS3 SCALE Supabase project after deployment. */
#define PS3SCALE_SUPPORT_URL "https://YOUR_PROJECT.supabase.co/functions/v1/ps3scale-support"
#define PS3SCALE_PS2_CONFIG_INDEX_PATH "/dev_hdd0/game/PS3SCALE1/USRDIR/ps2_config_index.tsv"
#define PS3SCALE_PS2_CLASSICS_CONFIG_DIR "/dev_hdd0/game/PS2CONFIG/USRDIR/CONFIG/ENC"
#define PS3SCALE_PS2_CONFIG_DOWNLOAD_LIMIT (PS3SCALE_PS2_CONFIG_MAX_BYTES * 2u)

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
static int g_music_mode = 0; /* 0=alternate both, 1=Music 1 only, 2=Music 2 only. */
static int g_music_next_track = 1;
static int g_music_start_track = 1;
static volatile int g_music_current_track = 0;
static int g_music_file_open = 0;
static volatile int g_music_track1_missing = 0;
static int g_music1_imported_from_usb = 0;
static unsigned int g_music_fade_remaining = 0;
static int g_settings_selected = 0;
static int g_animation_enabled = 1;
static unsigned long g_frame_counter = 0;
static char g_settings_message[128] = "SETTINGS ARE STORED ON THE PS3 HDD.";
static char g_details_message[128] = "NO VERIFIED GRAPHICS PROFILE FOR THIS VERSION.";
static int g_apollo_installed = 0;
static int g_artemis_installed = 0;
static int g_webman_installed = 0;
static char g_tools_message[96] = "CROSS: REFRESH TOOL STATUS.";
#define SETTINGS_COUNT 8
#define PS3SCALE_SETTINGS_PATH "/dev_hdd0/game/PS3SCALE1/USRDIR/settings.cfg"
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
static int g_ui_font_height = 22;

#define PS3SCALE_CATALOG_CACHE_PATH "/dev_hdd0/game/PS3SCALE1/USRDIR/graphics_catalog.tsv"
#define PS3SCALE_CATALOG_DOWNLOAD_PATH "/dev_hdd0/game/PS3SCALE1/USRDIR/graphics_catalog.download"
#define PS3SCALE_CATALOG_URL "https://raw.githubusercontent.com/Vikik0/PS3-SCALE/feature/online-graphics-database/PS3-SCALE-Alpha-0.2/data/online_profiles.tsv"
#define PS3SCALE_CATALOG_SYNC_SECONDS (24 * 60 * 60)
#define PS3SCALE_COMMUNITY_INDEX_CACHE_PATH "/dev_hdd0/game/PS3SCALE1/USRDIR/community_game_files.tsv"
#define PS3SCALE_COMMUNITY_INDEX_DOWNLOAD_PATH "/dev_hdd0/game/PS3SCALE1/USRDIR/community_game_files.download"
#define PS3SCALE_COMMUNITY_INDEX_URL "https://raw.githubusercontent.com/Vikik0/PS3-SCALE/feature/online-graphics-database/PS3-SCALE-Alpha-0.2/data/community_game_files.tsv"
#define PS3SCALE_COMMUNITY_INDEX_DOWNLOAD_LIMIT (2u * 1024u * 1024u)
#define PS3SCALE_MAX_ONLINE_RESULTS (PS3SCALE_COMMUNITY_MAX_FILES + PS3SCALE_MAX_GAMES + PS3SCALE_PS2_CONFIG_MAX_ENTRIES)

typedef enum online_result_kind {
    ONLINE_RESULT_PROFILE = 0,
    ONLINE_RESULT_COMMUNITY = 1,
    ONLINE_RESULT_PS2_CONFIG = 2
} online_result_kind_t;

typedef struct online_result {
    unsigned short game_index;
    unsigned short file_index;
    unsigned char kind;
} online_result_t;

static ps3scale_catalog_t g_catalog;
static ps3scale_catalog_t g_downloaded_catalog;
static ps3scale_community_index_t g_community_index;
static ps3scale_community_index_t g_downloaded_community_index;
static int g_community_ready = 0;
static online_result_t g_online_results[PS3SCALE_MAX_ONLINE_RESULTS];
static size_t g_online_result_count = 0;
static size_t g_online_profile_result_count = 0;
static size_t g_online_community_result_count = 0;
static size_t g_online_ps2_config_result_count = 0;
static size_t g_online_games_searched = 0;
static volatile int g_catalog_sync_running = 0;
static volatile int g_catalog_sync_done = 0;
static volatile int g_catalog_sync_result = 0;
static int g_catalog_thread_started = 0;
static sys_ppu_thread_t g_catalog_thread;
static int g_catalog_ready = 0;
static int g_online_selected = 0;
enum {
    ONLINE_FILTER_GRAPHICS_EXACT = 0,
    ONLINE_FILTER_GRAPHICS_ANY = 1,
    ONLINE_FILTER_ALL_EXACT = 2,
    ONLINE_FILTER_ALL_ANY = 3,
    ONLINE_FILTER_MODE_COUNT = 4
};
static int g_online_filter_mode = ONLINE_FILTER_GRAPHICS_EXACT;
static char g_online_query[64] = "";
static char g_scan_search_stash[64] = "";
static int g_keyboard_online_mode = 0;
static int g_profile_confirm_pending = 0;
static char g_catalog_status[96] = "LOCAL PROFILE + GITHUB INDEX NOT LOADED";
static char g_online_message[128] = "SELECT A GAME RESULT OR PRESS START TO REFRESH GITHUB.";
static void rebuild_online_results(void);
static volatile int g_game_launch_running = 0;
static volatile int g_game_launch_done = 0;
static volatile int g_game_launch_result = -1;
static int g_game_launch_thread_started = 0;
static int g_game_launch_profile_armed = 0;
static sys_ppu_thread_t g_game_launch_thread;
static ps3scale_game_t g_game_launch_target;
static volatile int g_profile_arm_running = 0;
static volatile int g_profile_arm_done = 0;
static volatile int g_profile_arm_result = -1;
static int g_profile_arm_thread_started = 0;
static sys_ppu_thread_t g_profile_arm_thread;
static ps3scale_profile_t g_profile_arm_target;

static void clamp_selection(void);
static void scan_library(void);
static int find_mounted_game(const ps3scale_game_t *selected,
                             ps3scale_game_t *mounted);
static int find_installed_game(const ps3scale_game_t *selected);
static void service_game_launch(void);
static void service_profile_arm(void);
static void service_ps2_config_download(void);
static void start_ps2_config_download(void);
static const ps3scale_ps2_config_entry_t *ps2_config_for_game(const ps3scale_game_t *game);
static const char *ps2_config_preferred_type(void);
static void service_ps2_config_download(void);
static void start_ps2_config_download(void);
static const ps3scale_ps2_config_entry_t *ps2_config_for_game(const ps3scale_game_t *game);

static void app_sysutil_callback(u64 status, u64 param, void *userdata)
{
    (void)param; (void)userdata;
    if (status == SYSUTIL_EXIT_GAME) g_exit_requested = 1;
}

static int open_search_keyboard(void)
{
    g_keyboard_online_mode = 0;
    snprintf(g_search_backup, sizeof(g_search_backup), "%s", g_search);
    g_keyboard_selected = 0;
    g_screen = SCREEN_KEYBOARD;
    return 0;
}

static int open_online_search_keyboard(void)
{
    snprintf(g_scan_search_stash, sizeof(g_scan_search_stash), "%s", g_search);
    snprintf(g_search_backup, sizeof(g_search_backup), "%s", g_online_query);
    snprintf(g_search, sizeof(g_search), "%s", g_online_query);
    g_keyboard_online_mode = 1;
    g_keyboard_selected = 0;
    g_screen = SCREEN_KEYBOARD;
    return 0;
}

static int open_support_keyboard(int field)
{
    g_keyboard_online_mode = 0;
    g_keyboard_support_mode = 1;
    g_support_selected = field;
    snprintf(g_search_backup, sizeof(g_search_backup), "%s", g_search);
    snprintf(g_search, sizeof(g_search), "%s",
             field == 0 ? g_support_title : g_support_details);
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
    g_ui_font_height = height;
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

static u32 ui_theme_color(u32 color)
{
    u32 rgb = color & 0xffffff00u;
    u32 alpha = color & 0x000000ffu;
    switch (rgb) {
        case 0xcaff3900u: rgb = 0x875be800u; break;
        case 0x9ee56a00u: rgb = 0x2c9f8e00u; break;
        case 0xffffff00u: rgb = 0x29243f00u; break;
        case 0xdce3e800u: rgb = 0x514b6900u; break;
        case 0x9aa6ad00u: rgb = 0x77718f00u; break;
        case 0x6f7b8300u: rgb = 0x9992ad00u; break;
        case 0xffcc6600u: rgb = 0xc58a4d00u; break;
        case 0xff7b7b00u: rgb = 0xd65d8500u; break;
        case 0x0d172200u: rgb = 0xf3efff00u; break;
        case 0x111b2500u: rgb = 0xf8f5ff00u; break;
        case 0x111e1800u: rgb = 0xeee8ff00u; break;
        case 0x14261d00u: rgb = 0xe9e2ff00u; break;
        case 0x173a3500u: rgb = 0xe8e0ff00u; break;
        case 0x1c594d00u: rgb = 0xe5dcff00u; break;
        case 0x101b2700u: rgb = 0xf4f1ff00u; break;
        case 0x26445c00u: rgb = 0xc6b8ed00u; break;
        case 0x34465400u: rgb = 0xc6b8ed00u; break;
        case 0x344f6300u: rgb = 0xc6b8ed00u; break;
        case 0x42647a00u: rgb = 0xc8bee800u; break;
        case 0x8bd45000u: rgb = 0x875be800u; break;
        case 0x4aafca00u: rgb = 0x875be800u; break;
        case 0x65c8e600u: rgb = 0xb49af700u; break;
        case 0x397fb600u: rgb = 0x9079dc00u; break;
        case 0x55b9d600u: rgb = 0x9c86e700u; break;
        default: break;
    }
    return rgb | alpha;
}

static u32 color_with_alpha(u32 color, unsigned alpha)
{
    return (color & 0xffffff00u) | (alpha & 0xffu);
}

static void draw_text(float x, float y, u32 color, const char *text)
{
    char translated[512];
    char glyph_text[512];
    const char *source = text ? text : "";
    size_t i, length;
    int contains_arabic = 0;
    int arabic_width = (g_ui_font_width * 18 + 7) / 14;

    if (arabic_width < g_ui_font_width)
        arabic_width = g_ui_font_width;

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

    SetFontColor(ui_theme_color(color), 0x00000000);
    if (g_extended_font_ready) {
        float cursor = x;

        for (i = 0; i < length; i++) {
            unsigned char glyph = (unsigned char)glyph_text[i];
            int font_index = ps3scale_locale_glyph_font_index(glyph);

            if (glyph == ' ') {
                cursor += contains_arabic ?
                    ((float)arabic_width * 0.5f) :
                    (float)g_ui_font_width;
                continue;
            }

            SetCurrentFont(font_index);
            if (ps3scale_locale_is_arabic_glyph(glyph)) {
                float scale_x =
                    (float)arabic_width / (float)PS3SCALE_FONT_WIDTH;
                float ink_width =
                    (float)g_extended_glyph_ink_width[glyph];
                unsigned min_x = g_extended_glyph_min_x[glyph];
                unsigned char next_glyph =
                    (i + 1u < length) ?
                    (unsigned char)glyph_text[i + 1u] : 0;

                if (ink_width <= 0.0f)
                    ink_width = (float)PS3SCALE_FONT_WIDTH;

                /*
                 * Arabic glyphs in the 32x32 atlas contain more vertical
                 * padding than the compact ASCII font. Enlarge only their
                 * cell height (about 1.35x) and center it on the UI line so
                 * Arabic remains readable at both normal and compact sizes.
                 */
                {
                    int arabic_height =
                        (g_ui_font_height * 27 + 10) / 20;
                    float y_offset =
                        (float)(arabic_height - g_ui_font_height) * 0.5f;
                    SetFontSize(arabic_width, arabic_height);
                    DrawChar(cursor - ((float)min_x * scale_x),
                             y - y_offset, 0.0f, glyph);
                    SetFontSize(g_ui_font_width, g_ui_font_height);
                }
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


static int import_music1_from_usb(void)
{
    static const char *usb_paths[] = {
        "/dev_usb000/music1.mp3", "/dev_usb001/music1.mp3",
        "/dev_usb002/music1.mp3", "/dev_usb003/music1.mp3",
        "/dev_usb000/PS3SCALE/music1.mp3",
        "/dev_usb001/PS3SCALE/music1.mp3",
        "/dev_usb002/PS3SCALE/music1.mp3",
        "/dev_usb003/PS3SCALE/music1.mp3",
        "/dev_usb000/BIOS - Zorrovian [Official Visualiser] (1).mp3",
        "/dev_usb001/BIOS - Zorrovian [Official Visualiser] (1).mp3",
        "/dev_usb002/BIOS - Zorrovian [Official Visualiser] (1).mp3",
        "/dev_usb003/BIOS - Zorrovian [Official Visualiser] (1).mp3",
        "/dev_usb000/BIOS - Zorrovian [Official Visualiser] (1)(2).mp3",
        "/dev_usb001/BIOS - Zorrovian [Official Visualiser] (1)(2).mp3",
        "/dev_usb002/BIOS - Zorrovian [Official Visualiser] (1)(2).mp3",
        "/dev_usb003/BIOS - Zorrovian [Official Visualiser] (1)(2).mp3",
        "/dev_usb000/PS3SCALE/BIOS - Zorrovian [Official Visualiser] (1).mp3",
        "/dev_usb001/PS3SCALE/BIOS - Zorrovian [Official Visualiser] (1).mp3",
        "/dev_usb002/PS3SCALE/BIOS - Zorrovian [Official Visualiser] (1).mp3",
        "/dev_usb003/PS3SCALE/BIOS - Zorrovian [Official Visualiser] (1).mp3",
        "/dev_usb000/PS3SCALE/BIOS - Zorrovian [Official Visualiser] (1)(2).mp3",
        "/dev_usb001/PS3SCALE/BIOS - Zorrovian [Official Visualiser] (1)(2).mp3",
        "/dev_usb002/PS3SCALE/BIOS - Zorrovian [Official Visualiser] (1)(2).mp3",
        "/dev_usb003/PS3SCALE/BIOS - Zorrovian [Official Visualiser] (1)(2).mp3"
    };
    char temp_path[PS3SCALE_PATH_MAX];
    unsigned char buffer[32768];
    FILE *existing;
    size_t i;

    if (!g_mpg123)
        return -1;

    existing = fopen(PS3SCALE_MUSIC1_PATH, "rb");
    if (existing) {
        fclose(existing);
        if (mpg123_open(g_mpg123, PS3SCALE_MUSIC1_PATH) == MPG123_OK) {
            mpg123_close(g_mpg123);
            return 0;
        }
        (void)remove(PS3SCALE_MUSIC1_PATH);
    }

    if (snprintf(temp_path, sizeof(temp_path), "%s.tmp",
                 PS3SCALE_MUSIC1_PATH) >= (int)sizeof(temp_path))
        return -1;

    for (i = 0; i < sizeof(usb_paths) / sizeof(usb_paths[0]); i++) {
        FILE *source = fopen(usb_paths[i], "rb");
        FILE *destination;
        size_t total = 0;
        int valid = 1;

        if (!source)
            continue;
        destination = fopen(temp_path, "wb");
        if (!destination) {
            fclose(source);
            return -1;
        }

        for (;;) {
            size_t got = fread(buffer, 1, sizeof(buffer), source);
            if (got > 0) {
                if (total > (20u * 1024u * 1024u) - got ||
                    fwrite(buffer, 1, got, destination) != got) {
                    valid = 0;
                    break;
                }
                total += got;
            }
            if (got < sizeof(buffer)) {
                if (ferror(source))
                    valid = 0;
                break;
            }
        }

        if (fflush(destination) != 0)
            valid = 0;
        if (fclose(source) != 0)
            valid = 0;
        if (fclose(destination) != 0)
            valid = 0;

        /* Verify the copied file is a decodable MP3 before replacing anything. */
        if (valid && total >= 16384u &&
            mpg123_open(g_mpg123, temp_path) == MPG123_OK) {
            mpg123_close(g_mpg123);
            if (rename(temp_path, PS3SCALE_MUSIC1_PATH) == 0) {
                g_music1_imported_from_usb = 1;
                return 1;
            }
        }
        (void)remove(temp_path);
    }
    return -1;
}

static int open_music_track(int track)
{
    const char *path = (track == 1) ? PS3SCALE_MUSIC1_PATH :
                                      PS3SCALE_MUSIC2_PATH;
    long rate = 0;
    int channels = 0;
    int encoding = 0;

    if (!g_mpg123 || !path || !path[0])
        return -1;
    if (g_music_file_open) {
        (void)mpg123_close(g_mpg123);
        g_music_file_open = 0;
    }
    if (mpg123_open(g_mpg123, path) != MPG123_OK)
        return -1;
    g_music_file_open = 1;
    if (mpg123_getformat(g_mpg123, &rate, &channels, &encoding) != MPG123_OK ||
        rate != 48000 || channels != MPG123_STEREO ||
        encoding != MPG123_ENC_SIGNED_16) {
        (void)mpg123_close(g_mpg123);
        g_music_file_open = 0;
        return -1;
    }
    g_music_current_track = track;
    return 0;
}

static int open_background_music(void)
{
    int error = 0;
    int requested_track;

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

    requested_track = (g_music_mode == 1) ? 1 :
                      (g_music_mode == 2) ? 2 : g_music_start_track;
    g_music_track1_missing = 0;
    g_music1_imported_from_usb = 0;
    if (g_music_mode != 2)
        (void)import_music1_from_usb();
    if (open_music_track(requested_track) != 0) {
        if (requested_track == 1) {
            g_music_track1_missing = 1;
            if (open_music_track(2) != 0)
                return -1;
        } else if (open_music_track(1) != 0) {
            return -1;
        }
    }
    g_music_fade_remaining = 48000u;
    return 0;
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
            if (g_music_mode == 0) {
                int next_track = (g_music_current_track == 1) ? 2 : 1;
                if (open_music_track(next_track) != 0) {
                    if (next_track == 1) g_music_track1_missing = 1;
                    if (open_music_track(2) != 0) break;
                }
                g_music_fade_remaining = 48000u;
            } else {
                if (mpg123_seek(g_mpg123, 0, SEEK_SET) < 0) break;
                g_music_fade_remaining = 48000u;
            }
            continue;
        }
        if (rc == MPG123_ERR || (decoded == 0 && rc == MPG123_OK)) break;
    }
    for (i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        float fade = 1.0f;
        if (g_music_fade_remaining > 0) {
            fade = 1.0f -
                ((float)g_music_fade_remaining / 48000.0f);
            g_music_fade_remaining--;
            if (fade < 0.0f) fade = 0.0f;
        }
        output[i * 2] =
            ((float)pcm[i * 2] / 32768.0f) * gain * fade;
        output[i * 2 + 1] =
            ((float)pcm[i * 2 + 1] / 32768.0f) * gain * fade;
    }
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
        if (g_music_file_open)
            (void)mpg123_close(g_mpg123);
        g_music_file_open = 0;
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

    if (!g_music_enabled || g_music_playing)
        return 0;
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
    tiny3d_VertexColor(ui_theme_color(color));
    tiny3d_VertexPos(x + w, y, 65535.0f);
    tiny3d_VertexPos(x + w, y + h, 65535.0f);
    tiny3d_VertexPos(x, y + h, 65535.0f);
    tiny3d_End();
}

static void draw_outline_rect(float x, float y, float w, float h, u32 color)
{
    tiny3d_SetPolygon(TINY3D_LINE_LOOP);
    tiny3d_VertexPos(x, y, 65535.0f);
    tiny3d_VertexColor(ui_theme_color(color));
    tiny3d_VertexPos(x + w, y, 65535.0f);
    tiny3d_VertexPos(x + w, y + h, 65535.0f);
    tiny3d_VertexPos(x, y + h, 65535.0f);
    tiny3d_End();
}

static void draw_icon_line(float x1, float y1, float x2, float y2, u32 color)
{
    tiny3d_SetPolygon(TINY3D_LINES);
    tiny3d_VertexPos(x1, y1, 65535.0f);
    tiny3d_VertexColor(ui_theme_color(color));
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
            tiny3d_VertexColor(ui_theme_color(color));
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
            tiny3d_VertexColor(ui_theme_color(color));
    }
    tiny3d_End();
}

static void draw_menu_icon(int index, float x, float y, int selected)
{
    u32 ink = selected ? 0xcaff39ff : 0x7c859aff;
    u32 tile = selected ? 0xeeeaffff : 0xf8f9fcff;
    u32 border = selected ? 0x8068e6ff : 0xd9deebff;
    draw_solid_rect(x, y, 42.0f, 42.0f, tile);
    draw_outline_rect(x, y, 42.0f, 42.0f, border);
    if (index == 0) {
        draw_circle_outline(x + 16.0f, y + 16.0f, 9.0f, ink);
        draw_icon_line(x + 23.0f, y + 23.0f, x + 33.0f, y + 33.0f, ink);
        draw_icon_line(x + 12.0f, y + 14.0f, x + 19.0f, y + 14.0f, ink);
        draw_icon_line(x + 12.0f, y + 18.0f, x + 18.0f, y + 18.0f, ink);
    } else if (index == 1) {
        draw_solid_rect(x + 8.0f, y + 9.0f, 26.0f, 7.0f, ink);
        draw_solid_rect(x + 8.0f, y + 18.0f, 26.0f, 7.0f, ink);
        draw_solid_rect(x + 8.0f, y + 27.0f, 26.0f, 7.0f, ink);
    } else if (index == 2) {
        draw_gear_outline(x + 21.0f, y + 21.0f, 15.0f, ink);
        draw_circle_outline(x + 21.0f, y + 21.0f, 5.0f, ink);
    } else if (index == 3) {
        draw_circle_outline(x + 21.0f, y + 21.0f, 15.0f, ink);
        draw_icon_line(x + 15.0f, y + 13.0f, x + 25.0f, y + 13.0f, ink);
        draw_icon_line(x + 25.0f, y + 13.0f, x + 28.0f, y + 19.0f, ink);
        draw_icon_line(x + 28.0f, y + 19.0f, x + 21.0f, y + 24.0f, ink);
        draw_icon_line(x + 21.0f, y + 24.0f, x + 21.0f, y + 29.0f, ink);
        draw_solid_rect(x + 19.5f, y + 33.0f, 3.0f, 3.0f, ink);
    } else if (index == 4) {
        draw_gear_outline(x + 21.0f, y + 21.0f, 14.0f, ink);
        draw_icon_line(x + 8.0f, y + 34.0f, x + 34.0f, y + 8.0f, ink);
    } else {
        draw_circle_outline(x + 21.0f, y + 21.0f, 15.0f, ink);
        draw_solid_rect(x + 19.5f, y + 15.0f, 3.0f, 12.0f, ink);
        draw_solid_rect(x + 19.5f, y + 10.0f, 3.0f, 3.0f, ink);
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
        tiny3d_VertexColor(ui_theme_color(color));
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
    tiny3d_VertexColor(ui_theme_color(color));
    tiny3d_VertexPos(px + 0.72f, py, 0.0f);
    tiny3d_VertexColor(ui_theme_color(color));
    tiny3d_VertexPos(px, py - 0.72f, 0.0f);
    tiny3d_VertexColor(ui_theme_color(color));
    tiny3d_VertexPos(px, py + 0.72f, 0.0f);
    tiny3d_VertexColor(ui_theme_color(color));
    tiny3d_VertexPos(px, py, -0.72f);
    tiny3d_VertexColor(ui_theme_color(color));
    tiny3d_VertexPos(px, py, 0.72f);
    tiny3d_VertexColor(ui_theme_color(color));
    tiny3d_End();
}

static void draw_stair_face(float ax, float ay, float az,
                            float bx, float by, float bz,
                            float cx, float cy, float cz,
                            float dx, float dy, float dz, u32 color)
{
    tiny3d_SetPolygon(TINY3D_QUADS);
    tiny3d_VertexPos(ax, ay, az); tiny3d_VertexColor(color);
    tiny3d_VertexPos(bx, by, bz); tiny3d_VertexColor(color);
    tiny3d_VertexPos(cx, cy, cz); tiny3d_VertexColor(color);
    tiny3d_VertexPos(dx, dy, dz); tiny3d_VertexColor(color);
    tiny3d_End();
}

static void draw_stair_tread(float width, float height, float depth, int index)
{
    float x = width * 0.5f;
    float y = height * 0.5f;
    float z = depth * 0.5f;
    u32 front = (index % 3 == 0) ? 0xded8eeff : 0xe7e2f3ff;
    u32 side = (index % 2 == 0) ? 0xcac0e2ff : 0xd5cceaff;

    draw_stair_face(-x, y, -z, x, y, -z, x, y, z, -x, y, z,
                    0xfffeffff);
    draw_stair_face(-x, -y, z, x, -y, z, x, y, z, -x, y, z,
                    front);
    draw_stair_face(x, -y, -z, x, -y, z, x, y, z, x, y, -z,
                    side);
    draw_stair_face(-x, -y, -z, -x, y, -z, -x, y, z, -x, -y, z,
                    0xf1edfbff);
    draw_stair_face(-x, -y, -z, x, -y, -z, x, y, -z, -x, y, -z,
                    0xe6e0f3ff);
}


static u32 scene_blend(u32 a, u32 b, float t)
{
    int ar = (int)((a >> 24) & 255u), ag = (int)((a >> 16) & 255u);
    int ab = (int)((a >> 8) & 255u);
    int br = (int)((b >> 24) & 255u), bg = (int)((b >> 16) & 255u);
    int bb = (int)((b >> 8) & 255u);
    int r, g, blue;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    r = ar + (int)((float)(br - ar) * t);
    g = ag + (int)((float)(bg - ag) * t);
    blue = ab + (int)((float)(bb - ab) * t);
    return ((u32)r << 24) | ((u32)g << 16) | ((u32)blue << 8) | 0xffu;
}

static void draw_scene_background(void)
{
    const float tau = 6.28318530718f;
    float phase = (float)(g_frame_counter % 21600ul) / 21600.0f * tau;
    float daylight = 0.82f + 0.12f * cosf(phase);
    float night = 1.0f - daylight;
    float dusk = 1.0f - fabsf(daylight - 0.82f) / 0.12f;
    u32 top, bottom, accent, glow;
    int i, wave;

    if (!g_animation_enabled) {
        daylight = 0.86f;
        night = 0.14f;
        dusk = 0.0f;
    }

    top = scene_blend(0xded8eeff, 0xfaf9ffff, daylight);
    bottom = scene_blend(0xcfc5e6ff, 0xf0ecf9ff, daylight);
    top = scene_blend(top, 0xe9defcff, dusk * 0.06f);
    bottom = scene_blend(bottom, 0xe0d4f5ff, dusk * 0.08f);
    accent = scene_blend(0x8b73c8ff, 0xb995ffff, daylight * 0.72f);
    glow = scene_blend(0x8a73b8ff, 0xf4eaffff, daylight);

    for (i = 0; i < 24; i++) {
        float t = (float)i / 23.0f;
        draw_solid_rect(0.0f, (float)i * 22.0f, 854.0f, 23.0f,
                        scene_blend(top, bottom, t));
    }

    /* Soft orbital halo and light ribbons: every motion is phase-periodic. */
    draw_circle_outline(738.0f + cosf(phase) * 12.0f,
                        112.0f + sinf(phase) * 7.0f,
                        74.0f, glow);
    draw_circle_outline(738.0f + cosf(phase) * 12.0f,
                        112.0f + sinf(phase) * 7.0f,
                        61.0f, accent);
    draw_circle_outline(738.0f + cosf(phase) * 12.0f,
                        112.0f + sinf(phase) * 7.0f,
                        48.0f, 0xe8ddffff);

    for (wave = 0; wave < 3; wave++) {
        float previous_x = 0.0f;
        float previous_y = 0.0f;
        int segment;
        for (segment = 0; segment <= 28; segment++) {
            float x = (float)segment * (854.0f / 28.0f);
            float y = 430.0f - (float)wave * 42.0f +
                sinf(phase + x * 0.0065f + (float)wave * 0.85f) *
                (8.0f + (float)wave * 2.0f);
            if (segment > 0)
                draw_icon_line(previous_x, previous_y, x, y,
                               scene_blend(0xc4b1f5ff, 0xffffffcc,
                                           daylight * 0.72f));
            previous_x = x;
            previous_y = y;
        }
    }

    for (i = 0; i < 22; i++) {
        float x = (float)((i * 137 + 31) % 842);
        float y = (float)((i * 83 + 19) % 470) +
                  sinf(phase + (float)i * 0.73f) * 3.0f;
        float pulse = 0.45f + 0.55f *
                      sinf(phase * 2.0f + (float)i * 1.17f);
        draw_solid_rect(x, y, (i % 5 == 0) ? 3.0f : 2.0f,
                        (i % 5 == 0) ? 3.0f : 2.0f,
                        scene_blend(0x9e8bcf00, 0xffffffff,
                                    pulse * daylight * 0.65f));
    }

    for (i = 0; i < 14; i++) {
        float x = (float)((i * 173 + 67) % 840);
        float y = (float)((i * 97 + 33) % 300);
        draw_solid_rect(x, y, 2.0f, 2.0f,
                        scene_blend(0x66558a00, 0xf6edffcc, night * 0.82f));
    }
    (void)dusk;
}

static void draw_home_ambient(void)
{
    MATRIX projection, rotation, scale, translation, model;
    const float tau = 6.28318530718f;
    const float phase =
        (float)(g_frame_counter % 21600ul) / 21600.0f * tau;
    const float bob = sinf(phase) * 0.12f;
    const float center_x = -5.8f;
    const float center_z = 25.0f;
    int i;

    tiny3d_Project3D();
    projection = MatrixProjPerspective(68.0f, 16.0f / 9.0f,
                                       0.1f, 240.0f);
    tiny3d_SetProjectionMatrix(&projection);
    tiny3d_SetLightsOff();

    /* Thirty-two low-poly treads form a slow, continuously turning spiral. */
    for (i = 0; i < 32; i++) {
        float angle = (float)i * 0.235f + phase;
        float radius = 4.5f +
            sinf(phase + (float)i * 0.15f) * 0.045f;
        float y = ((float)i - 15.5f) * 0.46f + bob;
        rotation = MatrixMultiply(MatrixRotationX(0.76f),
                                  MatrixRotationY(angle));
        scale = MatrixScale(2.0f, 2.0f, 2.0f);
        translation = MatrixTranslation(center_x + cosf(angle) * radius,
                                         y,
                                         center_z + sinf(angle) * radius);
        model = MatrixMultiply(MatrixMultiply(scale, rotation), translation);
        tiny3d_SetMatrixModelView(&model);
        draw_stair_tread(4.8f, 0.28f, 2.45f, i);
    }

    /* A fine lilac edge traces the spiral; the cycle wraps without a jump. */
    tiny3d_SetMatrixModelView(NULL);
    tiny3d_SetPolygon(TINY3D_LINE_STRIP);
    for (i = 0; i <= 64; i++) {
        float angle = (float)i * 0.115f + phase;
        float y = ((float)i * 0.5f - 15.5f) * 0.46f + bob;
        tiny3d_VertexPos(center_x + cosf(angle) * 4.58f,
                         y,
                         center_z + sinf(angle) * 4.58f);
        tiny3d_VertexColor(0xc0a8ff88);
    }
    tiny3d_End();

    draw_ambient_ring(10.0f, 1.42f, phase,
                      center_x + 1.5f, -0.5f, center_z,
                      0xb9a4ff50);
    draw_ambient_satellite(12.0f, phase, 0.52f, phase,
                           center_x + 1.5f, 0.0f, center_z,
                           0xc6b6ff88);

    tiny3d_Project2D();
    tiny3d_SetMatrixModelView(NULL);
}

static void set_settings_message(const char *message)
{
    if (!message) message = "";
    snprintf(g_settings_message, sizeof(g_settings_message), "%s", message);
}

static void set_details_message(const char *message)
{
    if (!message) message = "";
    snprintf(g_details_message, sizeof(g_details_message), "%s", message);
}


#define PS3SCALE_PROFILE_CACHE_FORMAT "PS3SCALE_PROFILE_DATA_V1"

static void load_catalog_cache(void)
{
    int count = ps3scale_catalog_load(PS3SCALE_CATALOG_CACHE_PATH, &g_catalog);
    int community_count = ps3scale_community_index_load(
        PS3SCALE_COMMUNITY_INDEX_CACHE_PATH, &g_community_index);

    g_catalog_ready = (count > 0);
    g_community_ready = (community_count > 0);

    if (g_catalog_ready && g_community_ready) {
        snprintf(g_catalog_status, sizeof(g_catalog_status),
                 "LOCAL PROFILE + GITHUB INDEX READY");
    } else if (g_catalog_ready) {
        snprintf(g_catalog_status, sizeof(g_catalog_status),
                 "LOCAL PROFILE DATABASE READY");
    } else if (g_community_ready) {
        snprintf(g_catalog_status, sizeof(g_catalog_status),
                 "GITHUB COMMUNITY INDEX READY");
    } else {
        snprintf(g_catalog_status, sizeof(g_catalog_status),
                 "NO VALID LOCAL DATABASE");
    }
    rebuild_online_results();
}

static int catalog_cache_is_stale(void)
{
    struct stat info;
    time_t now = time(NULL);

    if (stat(PS3SCALE_CATALOG_CACHE_PATH, &info) != 0)
        return 1;
    /* An unset console clock must not suppress the first online check. */
    if (now < (time_t)1600000000)
        return 1;
    if (now <= info.st_mtime)
        return 0;
    return (now - info.st_mtime) >= PS3SCALE_CATALOG_SYNC_SECONDS;
}

static int community_index_cache_is_stale(void)
{
    struct stat info;
    time_t now = time(NULL);

    if (stat(PS3SCALE_COMMUNITY_INDEX_CACHE_PATH, &info) != 0)
        return 1;
    if (now < (time_t)1600000000)
        return 1;
    if (now <= info.st_mtime)
        return 0;
    return (now - info.st_mtime) >= PS3SCALE_CATALOG_SYNC_SECONDS;
}

static void catalog_sync_worker(void *arg)
{
    int result = 0;
    (void)arg;
    g_catalog_sync_result = 0;
    ps3scale_community_index_free(&g_downloaded_community_index);

    if (ps3scale_online_download(PS3SCALE_CATALOG_URL,
                                 PS3SCALE_CATALOG_DOWNLOAD_PATH) == 0 &&
        ps3scale_catalog_load(PS3SCALE_CATALOG_DOWNLOAD_PATH,
                              &g_downloaded_catalog) > 0 &&
        rename(PS3SCALE_CATALOG_DOWNLOAD_PATH,
               PS3SCALE_CATALOG_CACHE_PATH) == 0) {
        result |= 1;
    } else {
        unlink(PS3SCALE_CATALOG_DOWNLOAD_PATH);
    }

    if (ps3scale_online_download_limited(
            PS3SCALE_COMMUNITY_INDEX_URL,
            PS3SCALE_COMMUNITY_INDEX_DOWNLOAD_PATH,
            PS3SCALE_COMMUNITY_INDEX_DOWNLOAD_LIMIT) == 0 &&
        ps3scale_community_index_load(
            PS3SCALE_COMMUNITY_INDEX_DOWNLOAD_PATH,
            &g_downloaded_community_index) > 0 &&
        rename(PS3SCALE_COMMUNITY_INDEX_DOWNLOAD_PATH,
               PS3SCALE_COMMUNITY_INDEX_CACHE_PATH) == 0) {
        result |= 2;
    } else {
        unlink(PS3SCALE_COMMUNITY_INDEX_DOWNLOAD_PATH);
        ps3scale_community_index_free(&g_downloaded_community_index);
    }

    g_catalog_sync_result = result;

    /* Publish completion before clearing running so UI actions never
     * overlap the final network teardown window. */
    g_catalog_sync_done = 1;
    g_catalog_sync_running = 0;
}

static void start_catalog_sync_worker(int force)
{
    int rc;

    if (g_catalog_sync_running || g_catalog_sync_done ||
        g_game_launch_running || g_game_launch_done ||
        g_profile_arm_running || g_profile_arm_done) {
        snprintf(g_online_message, sizeof(g_online_message),
                 "DATABASE SYNC OR GAME START IS ALREADY RUNNING.");
        return;
    }
    if (!force && !catalog_cache_is_stale() &&
        !community_index_cache_is_stale())
        return;

    g_catalog_sync_done = 0;
    g_catalog_sync_result = 0;
    g_catalog_sync_running = 1;
    snprintf(g_catalog_status, sizeof(g_catalog_status),
             "CONNECTING TO GITHUB DATABASE");
    snprintf(g_online_message, sizeof(g_online_message),
             "REFRESHING GITHUB PROFILE + COMMUNITY GAME INDEX...");
    rc = sysThreadCreate(&g_catalog_thread, catalog_sync_worker, NULL,
                         1500, 0x10000, THREAD_JOINABLE,
                         "PS3 SCALE Online Database");
    if (rc == 0) {
        g_catalog_thread_started = 1;
    } else {
        g_catalog_sync_running = 0;
        g_catalog_sync_done = 0;
        snprintf(g_catalog_status, sizeof(g_catalog_status),
                 "ONLINE THREAD COULD NOT START");
    }
}

static void service_catalog_sync(void)
{
    int result;
    if (!g_catalog_sync_done)
        return;

    if (g_catalog_thread_started) {
        u64 thread_result = 0;
        (void)sysThreadJoin(g_catalog_thread, &thread_result);
        g_catalog_thread_started = 0;
    }

    result = g_catalog_sync_result;
    if (result & 1) {
        g_catalog = g_downloaded_catalog;
        g_catalog_ready = 1;
    }
    if (result & 2) {
        ps3scale_community_index_free(&g_community_index);
        g_community_index = g_downloaded_community_index;
        memset(&g_downloaded_community_index, 0,
               sizeof(g_downloaded_community_index));
        g_community_ready = 1;
    }

    rebuild_online_results();
    if (result == 3) {
        snprintf(g_catalog_status, sizeof(g_catalog_status),
                 "GITHUB PROFILE + GAME INDEX UPDATED");
        snprintf(g_online_message, sizeof(g_online_message),
                 "GITHUB SEARCH INDEX UPDATED. GAME NAMES WERE RESCANNED.");
    } else if (result == 1) {
        snprintf(g_catalog_status, sizeof(g_catalog_status),
                 "PROFILE DB UPDATED - GITHUB INDEX CACHED");
        snprintf(g_online_message, sizeof(g_online_message),
                 "PROFILE DB UPDATED. USING LAST VALID GITHUB INDEX.");
    } else if (result == 2) {
        snprintf(g_catalog_status, sizeof(g_catalog_status),
                 "GITHUB GAME INDEX UPDATED");
        snprintf(g_online_message, sizeof(g_online_message),
                 "COMMUNITY FILE INDEX UPDATED. EXACT PROFILES ARE UNCHANGED.");
    } else if (g_catalog_ready || g_community_ready) {
        snprintf(g_catalog_status, sizeof(g_catalog_status),
                 "UPDATE FAILED - USING LAST VALID CACHE");
        snprintf(g_online_message, sizeof(g_online_message),
                 "UPDATE FAILED. LAST VALID LOCAL DATABASES ARE STILL AVAILABLE.");
    } else {
        snprintf(g_catalog_status, sizeof(g_catalog_status),
                 "OFFLINE - NO VALID DATABASE");
        snprintf(g_online_message, sizeof(g_online_message),
                 "CONNECT THE PS3 TO THE INTERNET AND PRESS START TO RETRY.");
    }
    g_catalog_sync_done = 0;
}

static const ps3scale_profile_t *profile_for_game(const ps3scale_game_t *game)
{
    if (!g_catalog_ready || !game || !game_is_ps3(game))
        return NULL;
    return ps3scale_catalog_match_game(&g_catalog, game);
}

static int profile_script_state_for_game(const ps3scale_game_t *game,
                                         const ps3scale_profile_t *profile)
{
    if (!game || !game_is_ps3(game) || !game->title_id[0])
        return 0;
    if (profile && strcmp(profile->trust, "B_COMMUNITY") != 0)
        return ps3scale_profile_script_state(game->title_id, game->app_ver, NULL);
    return ps3scale_profile_script_state(
        game->title_id, game->app_ver, profile ? profile->patches : NULL);
}

static const char *online_filter_label(void)
{
    switch (g_online_filter_mode) {
    case ONLINE_FILTER_GRAPHICS_ANY: return "GRAPHICS + ANY REGION/VER";
    case ONLINE_FILTER_ALL_EXACT: return "ALL FILES + EXACT REGION/VER";
    case ONLINE_FILTER_ALL_ANY: return "ALL FILES + ANY REGION/VER";
    default: return "GRAPHICS + EXACT REGION/VER";
    }
}

static int online_filter_graphics_only(void)
{
    return g_online_filter_mode == ONLINE_FILTER_GRAPHICS_EXACT ||
           g_online_filter_mode == ONLINE_FILTER_GRAPHICS_ANY;
}

static int online_filter_exact_only(void)
{
    return g_online_filter_mode == ONLINE_FILTER_GRAPHICS_EXACT ||
           g_online_filter_mode == ONLINE_FILTER_ALL_EXACT;
}

static int online_result_matches_query(
    const ps3scale_game_t *game,
    const ps3scale_profile_t *profile,
    const ps3scale_community_file_t *community)
{
    const char *query = g_online_query;
    if (!query[0])
        return 1;
    if (game && (text_contains_ci(game->title, query) ||
                 text_contains_ci(game->title_id, query)))
        return 1;
    if (profile && (text_contains_ci(profile->title, query) ||
                    text_contains_ci(profile->effect, query) ||
                    text_contains_ci(profile->title_id, query)))
        return 1;
    if (community && (text_contains_ci(community->title, query) ||
                      text_contains_ci(community->title_id, query) ||
                      text_contains_ci(community->path, query) ||
                      text_contains_ci(community->repository, query) ||
                      text_contains_ci(community->description, query) ||
                      text_contains_ci(community->category, query)))
        return 1;
    return 0;
}

static void rebuild_online_results(void)
{
    size_t i, j, k;
    int graphics_only = online_filter_graphics_only();
    int exact_only = online_filter_exact_only();

    g_online_result_count = 0;
    g_online_profile_result_count = 0;
    g_online_community_result_count = 0;
    g_online_ps2_config_result_count = 0;
    g_online_games_searched = 0;

    for (i = 0; i < g_games.count; i++) {
        const ps3scale_game_t *game = &g_games.items[i];
        const ps3scale_profile_t *profile;
        if (game_is_ps3(game)) {
            g_online_games_searched++;
            if (!g_catalog_ready)
                continue;
            profile = ps3scale_catalog_match_game(&g_catalog, game);
            if (!profile || !online_result_matches_query(game, profile, NULL) ||
                g_online_result_count >= PS3SCALE_MAX_ONLINE_RESULTS)
                continue;
            g_online_results[g_online_result_count].game_index = (unsigned short)i;
            g_online_results[g_online_result_count].file_index = 0;
            g_online_results[g_online_result_count].kind = ONLINE_RESULT_PROFILE;
            g_online_result_count++;
            g_online_profile_result_count++;
        } else if (game->kind == PS3SCALE_GAME_PS2_ISO ||
                   game->kind == PS3SCALE_GAME_PS2_CLASSIC) {
            g_online_games_searched++;
        }
    }

    if (g_community_ready && g_community_index.items) {
        /* Installed-game matches are preferred; search-only matches are still shown. */
        for (j = 0; j < g_community_index.count; j++) {
            const ps3scale_community_file_t *file = &g_community_index.items[j];
            int matched_installed_game = 0;
            if (graphics_only && strcmp(file->category, "GRAPHICS_FPS") != 0)
                continue;

            for (i = 0; i < g_games.count; i++) {
                const ps3scale_game_t *game = &g_games.items[i];
                int matched;
                if (!game_is_ps3(game))
                    continue;
                matched = exact_only ?
                    ps3scale_community_file_matches_exact_game(file, game) :
                    ps3scale_community_file_matches_game(file, game);
                if (!matched || !online_result_matches_query(game, NULL, file))
                    continue;

                if (g_online_result_count < PS3SCALE_MAX_ONLINE_RESULTS) {
                    g_online_results[g_online_result_count].game_index = (unsigned short)i;
                    g_online_results[g_online_result_count].file_index = (unsigned short)j;
                    g_online_results[g_online_result_count].kind = ONLINE_RESULT_COMMUNITY;
                    g_online_result_count++;
                    g_online_community_result_count++;
                }
                matched_installed_game = 1;
                break;
            }

            if (!matched_installed_game && g_online_query[0] &&
                online_result_matches_query(NULL, NULL, file)) {
                int duplicate = 0;
                for (k = 0; k < g_online_result_count; k++) {
                    if (g_online_results[k].kind == ONLINE_RESULT_COMMUNITY &&
                        g_online_results[k].file_index == (unsigned short)j) {
                        duplicate = 1;
                        break;
                    }
                }
                if (!duplicate &&
                    g_online_result_count < PS3SCALE_MAX_ONLINE_RESULTS) {
                    g_online_results[g_online_result_count].game_index = 0xffffu;
                    g_online_results[g_online_result_count].file_index = (unsigned short)j;
                    g_online_results[g_online_result_count].kind = ONLINE_RESULT_COMMUNITY;
                    g_online_result_count++;
                    g_online_community_result_count++;
                }
            }
        }
    }

    /* PS2 CONFIG search is title-name based and does not require a local ISO. */
    if (g_online_query[0] && g_ps2_config_db.items) {
        for (j = 0; j < g_ps2_config_db.count; j++) {
            const ps3scale_ps2_config_entry_t *entry = &g_ps2_config_db.items[j];
            const ps3scale_ps2_config_entry_t *best;
            const char *preferred;
            int duplicate = 0;

            if (!text_contains_ci(entry->title, g_online_query) &&
                !text_contains_ci(entry->title_id, g_online_query))
                continue;
            if (strcmp(entry->type, "CUSTOM") != 0 &&
                strcmp(entry->type, "NET") != 0 &&
                strcmp(entry->type, "ENC") != 0)
                continue;

            preferred = strcmp(entry->type, "ENC") == 0 ?
                        "ENC" : ps2_config_preferred_type();
            best = ps3scale_ps2_config_db_find(
                &g_ps2_config_db, entry->title_id, preferred);
            if (best != entry)
                continue;

            for (k = 0; k < g_online_result_count; k++) {
                if (g_online_results[k].kind == ONLINE_RESULT_PS2_CONFIG &&
                    g_online_results[k].file_index == (unsigned short)j) {
                    duplicate = 1;
                    break;
                }
            }
            if (duplicate ||
                g_online_result_count >= PS3SCALE_MAX_ONLINE_RESULTS)
                continue;

            g_online_results[g_online_result_count].game_index = 0xffffu;
            g_online_results[g_online_result_count].file_index = (unsigned short)j;
            g_online_results[g_online_result_count].kind = ONLINE_RESULT_PS2_CONFIG;
            g_online_result_count++;
            g_online_ps2_config_result_count++;
        }
    }

    if (g_online_result_count == 0)
        g_online_selected = 0;
    else if ((size_t)g_online_selected >= g_online_result_count)
        g_online_selected = (int)g_online_result_count - 1;
}


static const ps3scale_game_t *online_result_at(
    size_t target,
    const ps3scale_profile_t **profile_out,
    const ps3scale_community_file_t **community_out,
    const ps3scale_ps2_config_entry_t **ps2_config_out)
{
    const online_result_t *result;
    const ps3scale_game_t *game;

    if (profile_out) *profile_out = NULL;
    if (community_out) *community_out = NULL;
    if (ps2_config_out) *ps2_config_out = NULL;
    if (target >= g_online_result_count)
        return NULL;

    result = &g_online_results[target];
    if (result->kind == ONLINE_RESULT_COMMUNITY) {
        if (community_out &&
            (size_t)result->file_index < g_community_index.count)
            *community_out = &g_community_index.items[result->file_index];
        if (result->game_index == 0xffffu)
            return NULL;
    } else if (result->kind == ONLINE_RESULT_PS2_CONFIG) {
        if (ps2_config_out &&
            (size_t)result->file_index < g_ps2_config_db.count)
            *ps2_config_out = &g_ps2_config_db.items[result->file_index];
        return NULL;
    }

    if ((size_t)result->game_index >= g_games.count)
        return NULL;
    game = &g_games.items[result->game_index];

    if (result->kind == ONLINE_RESULT_PROFILE && profile_out)
        *profile_out = profile_for_game(game);
    return game;
}


static int ps2_config_ids_match(const char *left, const char *right)
{
    char a[PS3SCALE_PS2_CONFIG_ID_MAX];
    char b[PS3SCALE_PS2_CONFIG_ID_MAX];
    if (!left || !right)
        return 0;
    if (ps3scale_ps2_config_normalize_id(left, a, sizeof(a)) != 0 ||
        ps3scale_ps2_config_normalize_id(right, b, sizeof(b)) != 0)
        return strcmp(left, right) == 0;
    return strcmp(a, b) == 0;
}

static void open_selected_online_match(void)
{
    const ps3scale_game_t *match;
    const ps3scale_profile_t *profile = NULL;
    const ps3scale_community_file_t *community = NULL;
    const ps3scale_ps2_config_entry_t *ps2_config = NULL;
    size_t i, count;

    match = online_result_at((size_t)g_online_selected, &profile,
                             &community, &ps2_config);
    if (ps2_config) {
        int found_local_game = 0;
        g_library = LIBRARY_PS2;
        g_search[0] = '\0';
        g_selected = 0;
        count = library_count(LIBRARY_PS2);
        for (i = 0; i < count; i++) {
            const ps3scale_game_t *candidate = library_game_at(i);
            const char *candidate_id;
            if (!candidate)
                continue;
            candidate_id = candidate->disc_id[0] ?
                           candidate->disc_id : candidate->title_id;
            if (ps2_config_ids_match(candidate_id, ps2_config->title_id)) {
                g_selected = (int)i;
                found_local_game = 1;
                break;
            }
        }
        if (found_local_game) {
            g_profile_confirm_pending = 0;
            g_config_preview = 0;
            set_details_message("VERIFIED PS2 CONFIG FOUND. TRIANGLE TO DOWNLOAD.");
            g_screen = SCREEN_DETAILS;
        } else {
            snprintf(g_online_message, sizeof(g_online_message),
                     "PS2 CONFIG FOUND FOR %.32s. SCAN THE MATCHING PS2 GAME FIRST.",
                     ps2_config->title);
        }
        return;
    }

    if (!match) {
        if (community)
            snprintf(g_online_message, sizeof(g_online_message),
                     "GITHUB FILE FOUND FOR %.32s. LOCAL GAME NOT SCANNED.",
                     community->title);
        else
            snprintf(g_online_message, sizeof(g_online_message),
                     "NO GITHUB RESULT IS SELECTED.");
        return;
    }

    g_library = LIBRARY_PS3;
    g_search[0] = '\0';
    g_selected = 0;
    count = library_count(LIBRARY_PS3);
    for (i = 0; i < count; i++) {
        const ps3scale_game_t *candidate = library_game_at(i);
        if (candidate &&
            (candidate == match ||
             (match->title_id[0] &&
              strcmp(candidate->title_id, match->title_id) == 0) ||
             strcmp(candidate->title, match->title) == 0)) {
            g_selected = (int)i;
            break;
        }
    }
    g_profile_confirm_pending = 0;
    if (profile)
        set_details_message("EXACT GRAPHICS PROFILE FOUND. REVIEW BEFORE INSTALL.");
    else if (community && profile_for_game(match))
        set_details_message("EXACT GRAPHICS PROFILE AVAILABLE. REVIEW BEFORE INSTALL.");
    else if (community && strcmp(community->category, "GRAPHICS_FPS") == 0)
        set_details_message("FPS NCL REFERENCE ONLY. NO SAFE AUTO-APPLY PROFILE.");
    else if (community)
        set_details_message("COMMUNITY GAME CODE ONLY. NOT A GRAPHICS ENHANCEMENT.");
    else
        set_details_message("GAME FOUND IN GITHUB INDEX.");
    g_screen = SCREEN_DETAILS;
}


static void profile_arm_worker(void *arg)
{
    (void)arg;

    if (ps3scale_webman_available() != 0) {
        g_profile_arm_result = -100;
    } else if (ps3scale_webman_ps3mapi_available() != 0) {
        g_profile_arm_result = -101;
    } else {
        g_profile_arm_result =
            ps3scale_profile_script_install(&g_profile_arm_target);
    }

    g_profile_arm_done = 1;
    g_profile_arm_running = 0;
}

static void service_profile_arm(void)
{
    int result;

    if (!g_profile_arm_done)
        return;

    if (g_profile_arm_thread_started) {
        u64 thread_result = 0;
        (void)sysThreadJoin(g_profile_arm_thread, &thread_result);
        g_profile_arm_thread_started = 0;
    }

    result = g_profile_arm_result;
    g_profile_arm_done = 0;
    g_profile_arm_running = 0;

    if (result == 0 || result == 1) {
        set_details_message("ONE-LAUNCH PROFILE ARMED. START THIS GAME.");
    } else if (result == -100) {
        set_details_message("WEBMAN MOD NOT RESPONDING. PROFILE NOT ARMED.");
    } else if (result == -101) {
        set_details_message("PS3MAPI NOT ACTIVE. PROFILE NOT ARMED.");
    } else if (result == -2) {
        set_details_message("A USER WEBMAN SCRIPT EXISTS. IT WAS NOT OVERWRITTEN.");
    } else if (result == -3) {
        set_details_message("MULTI-WRITE PROFILE DISABLED FOR AUTO APPLY SAFETY.");
    } else {
        set_details_message("PROFILE SCRIPT INSTALL FAILED. NO GAME FILES CHANGED.");
    }
}

static void confirm_profile_install(void)
{
    const ps3scale_game_t *game = selected_game();
    const ps3scale_profile_t *profile = profile_for_game(game);
    int result;

    if (!profile) {
        set_details_message("NO EXACT ONLINE PROFILE FOR THIS GAME UPDATE.");
        g_profile_confirm_pending = 0;
        return;
    }
    if (strcmp(profile->trust, "B_COMMUNITY_MANUAL") == 0) {
        set_details_message("MANUAL-ONLY PROFILE. ONE-LAUNCH APPLY IS NOT SUPPORTED.");
        g_profile_confirm_pending = 0;
        return;
    }
    if (profile->patch_count != 1) {
        set_details_message("MULTI-WRITE PROFILE DISABLED FOR AUTO APPLY SAFETY.");
        g_profile_confirm_pending = 0;
        return;
    }
    if (g_catalog_sync_running || g_catalog_sync_done ||
        g_game_launch_running || g_game_launch_done ||
        g_profile_arm_running || g_profile_arm_done) {
        set_details_message("WAIT FOR ONLINE DATABASE OR GAME ACTION TO FINISH.");
        g_profile_confirm_pending = 0;
        return;
    }

    if (game && game->kind == PS3SCALE_GAME_PS3_HDD) {
        if (!find_installed_game(game)) {
            set_details_message("GAME UPDATE CHANGED. RESCAN BEFORE APPLYING PROFILE.");
            g_profile_confirm_pending = 0;
            return;
        }
    } else {
        ps3scale_game_t mounted;
        if (!find_mounted_game(game, &mounted)) {
            set_details_message("MOUNT THE EXACT GAME VERSION BEFORE ARMING THIS PROFILE.");
            g_profile_confirm_pending = 0;
            return;
        }
    }
    g_profile_arm_target = *profile;
    g_profile_arm_result = -1;
    g_profile_arm_done = 0;
    g_profile_arm_running = 1;
    g_profile_confirm_pending = 0;
    set_details_message("CHECKING WEBMAN AND ARMING PROFILE.");

    result = sysThreadCreate(&g_profile_arm_thread, profile_arm_worker, NULL,
                             1500, 0x8000, THREAD_JOINABLE,
                             "PS3 SCALE Profile Arm");
    if (result == 0) {
        g_profile_arm_thread_started = 1;
    } else {
        g_profile_arm_running = 0;
        g_profile_arm_done = 0;
        set_details_message("COULD NOT START PROFILE WORKER. NO PATCH WAS ARMED.");
    }
}

static int find_mounted_game(const ps3scale_game_t *selected, ps3scale_game_t *mounted)
{
    ps3scale_game_list_t mounted_list;
    size_t i;

    if (!selected || !mounted || !selected->title_id[0])
        return 0;

    ps3scale_game_list_init(&mounted_list);
    if (ps3scale_scan_device("/dev_bdvd", &mounted_list) <= 0)
        return 0;

    for (i = 0; i < mounted_list.count; i++) {
        const ps3scale_game_t *candidate = &mounted_list.items[i];
        if (strcmp(candidate->title_id, selected->title_id) != 0)
            continue;
        if (selected->app_ver[0] &&
            (!candidate->app_ver[0] ||
             strcmp(candidate->app_ver, selected->app_ver) != 0))
            continue;
        *mounted = *candidate;
        return 1;
    }
    return 0;
}

static int find_installed_game(const ps3scale_game_t *selected)
{
    ps3scale_game_list_t installed_list;
    size_t i;

    if (!selected || selected->kind != PS3SCALE_GAME_PS3_HDD ||
        !selected->title_id[0] || !selected->app_ver[0])
        return 0;

    ps3scale_game_list_init(&installed_list);
    if (ps3scale_scan_device("/dev_hdd0", &installed_list) <= 0)
        return 0;

    for (i = 0; i < installed_list.count; i++) {
        const ps3scale_game_t *candidate = &installed_list.items[i];
        if (candidate->kind != PS3SCALE_GAME_PS3_HDD)
            continue;
        if (strcmp(candidate->title_id, selected->title_id) != 0 ||
            strcmp(candidate->app_ver, selected->app_ver) != 0 ||
            strcmp(candidate->path, selected->path) != 0)
            continue;
        return 1;
    }
    return 0;
}

static void game_launch_worker(void *arg)
{
    (void)arg;

    if (g_game_launch_profile_armed &&
        ps3scale_webman_ps3mapi_available() != 0) {
        g_game_launch_result = -2;
    } else {
        g_game_launch_result =
            ps3scale_webman_start_game(&g_game_launch_target);
    }

    g_game_launch_done = 1;
    g_game_launch_running = 0;
}

static void service_game_launch(void)
{
    if (!g_game_launch_done)
        return;

    if (g_game_launch_thread_started) {
        u64 thread_result = 0;
        (void)sysThreadJoin(g_game_launch_thread, &thread_result);
        g_game_launch_thread_started = 0;
    }

    g_game_launch_done = 0;
    g_game_launch_running = 0;

    if (g_game_launch_result == 0) {
        set_details_message("WEBMAN ACCEPTED START REQUEST.");
        g_exit_requested = 1;
    } else if (g_game_launch_result == -2) {
        set_details_message("PS3MAPI NOT ACTIVE. PROFILE WAS NOT APPLIED.");
    } else {
        set_details_message("WEBMAN START FAILED. APP STILL RESPONSIVE.");
    }
}

static void start_selected_game(void)
{
    const ps3scale_game_t *game = selected_game();
    const ps3scale_profile_t *profile = NULL;
    ps3scale_game_t mounted;
    int script_state = 0;
    int rc;
    int is_ps2_iso = game && game->kind == PS3SCALE_GAME_PS2_ISO;

    if (!game || (!game_is_ps3(game) && !is_ps2_iso)) {
        set_details_message("PS2 CLASSICS MUST BE STARTED WITH THEIR COMPATIBLE LAUNCHER.");
        return;
    }
    if (!is_ps2_iso && !game->title_id[0]) {
        set_details_message("TITLE ID NOT AVAILABLE. RESCAN THE GAME.");
        return;
    }
    if (g_game_launch_running || g_game_launch_done ||
        g_profile_arm_running || g_profile_arm_done ||
        g_ps2_config_download_running || g_ps2_config_download_done) {
        set_details_message("WAIT FOR THE CURRENT GAME OR CONFIG TASK TO FINISH.");
        return;
    }
    if (g_catalog_sync_running || g_catalog_sync_done) {
        set_details_message("WAIT FOR ONLINE DATABASE SYNC TO FINISH BEFORE STARTING.");
        return;
    }

    if (!is_ps2_iso) {
        profile = profile_for_game(game);
        script_state = profile_script_state_for_game(game, profile);
        if (script_state == -2) {
            set_details_message("STALE PROFILE SCRIPT. REMOVE IT BEFORE STARTING.");
            return;
        }

        /* A one-launch patch must still target the exact selected game revision. */
        if (script_state == 1) {
            if (game->kind == PS3SCALE_GAME_PS3_HDD) {
                if (!find_installed_game(game)) {
                    set_details_message("GAME UPDATE CHANGED. RESCAN BEFORE STARTING PROFILE.");
                    return;
                }
            } else if (!find_mounted_game(game, &mounted)) {
                set_details_message("MOUNT THE EXACT GAME VERSION BEFORE STARTING PROFILE.");
                return;
            }
        }
    }

    g_game_launch_target = *game;
    g_game_launch_profile_armed = !is_ps2_iso && (script_state == 1);
    g_game_launch_result = -1;
    g_game_launch_done = 0;
    g_game_launch_running = 1;

    if (is_ps2_iso)
        set_details_message("MOUNTING PS2 ISO VIA WEBMAN. WAIT FOR THE RESPONSE.");
    else if (script_state == 1)
        set_details_message("STARTING GAME WITH ONE-LAUNCH PROFILE.");
    else if (profile)
        set_details_message("PROFILE NOT ARMED. STARTING GAME UNMODIFIED.");
    else
        set_details_message("NO EXACT PROFILE. STARTING GAME UNMODIFIED.");

    rc = sysThreadCreate(&g_game_launch_thread, game_launch_worker, NULL,
                         1500, 0x8000, THREAD_JOINABLE,
                         "PS3 SCALE Game Launch");
    if (rc == 0) {
        g_game_launch_thread_started = 1;
    } else {
        g_game_launch_running = 0;
        g_game_launch_done = 0;
        set_details_message("COULD NOT START GAME LAUNCH WORKER. APP IS READY.");
    }
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
        else if (sscanf(line, "animation_v2=%d", &value) == 1)
            g_animation_enabled = value ? 1 : 0;
        else if (sscanf(line, "music_mode=%d", &value) == 1 &&
                 value >= 0 && value <= 2)
            g_music_mode = value;
        else if (sscanf(line, "music_next=%d", &value) == 1 &&
                 (value == 1 || value == 2))
            g_music_next_track = value;
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
    fprintf(fp, "animation_v2=%d\n", g_animation_enabled ? 1 : 0);
    fprintf(fp, "music_mode=%d\n", g_music_mode);
    fprintf(fp, "music_next=%d\n", g_music_next_track);
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

static int file_exists_readonly(const char *path)
{
    FILE *fp;
    if (!path || !path[0]) return 0;
    fp = fopen(path, "rb");
    if (!fp) return 0;
    fclose(fp);
    return 1;
}

static const char *ps2_config_preferred_type(void)
{
    if (g_hardware_info_valid &&
        g_hardware_info.family == PS3SCALE_HARDWARE_FAT) {
        if (strstr(g_hardware_info.model_series, "CECHC") ||
            strstr(g_hardware_info.model_series, "CECHE"))
            return "GX";
        return "SOFT";
    }
    return "NET";
}

static const ps3scale_ps2_config_entry_t *ps2_config_for_game(
    const ps3scale_game_t *game)
{
    const char *id;
    if (!game || !g_ps2_config_db.items)
        return NULL;

    if (game->kind == PS3SCALE_GAME_PS2_ISO) {
        id = game->disc_id;
        if (!id[0])
            return NULL;
        return ps3scale_ps2_config_db_find(
            &g_ps2_config_db, id, ps2_config_preferred_type());
    }

    if (game->kind == PS3SCALE_GAME_PS2_CLASSIC) {
        id = game->disc_id[0] ? game->disc_id : game->title_id;
        if (!id[0])
            return NULL;
        return ps3scale_ps2_config_db_find(&g_ps2_config_db, id, "ENC");
    }
    return NULL;
}

static void ps2_config_download_worker(void *arg)
{
    unsigned char data[PS3SCALE_PS2_CONFIG_DOWNLOAD_LIMIT];
    FILE *fp = NULL;
    long file_size = 0;
    int result = -1;
    struct stat st;

    (void)arg;
    if (ps3scale_online_download_limited(
            g_ps2_config_download_url,
            g_ps2_config_download_temp_path,
            PS3SCALE_PS2_CONFIG_DOWNLOAD_LIMIT) == 0) {
        fp = fopen(g_ps2_config_download_temp_path, "rb");
        if (!fp) {
            result = -3;
        } else {
            if (fseek(fp, 0, SEEK_END) == 0)
                file_size = ftell(fp);
            if (file_size < 0 ||
                (size_t)file_size > sizeof(data) ||
                fseek(fp, 0, SEEK_SET) != 0 ||
                fread(data, 1, (size_t)file_size, fp) != (size_t)file_size) {
                result = -3;
            } else if (ps3scale_ps2_config_validate_external(
                           data, (size_t)file_size,
                           g_ps2_config_download_type) != 0) {
                result = -4;
            } else if (stat(g_ps2_config_download_path, &st) == 0) {
                result = -2;
            } else if (rename(g_ps2_config_download_temp_path,
                              g_ps2_config_download_path) == 0) {
                result = 0;
            } else {
                result = -5;
            }
            fclose(fp);
            fp = NULL;
        }
    }

    if (result != 0)
        (void)unlink(g_ps2_config_download_temp_path);
    g_ps2_config_download_result = result;
    g_ps2_config_download_running = 0;
    g_ps2_config_download_done = 1;
}

static void start_ps2_config_download(void)
{
    const ps3scale_game_t *game = selected_game();
    const ps3scale_ps2_config_entry_t *entry = ps2_config_for_game(game);
    struct stat st;
    char url[512];
    char target[PS3SCALE_PATH_MAX];
    char temporary[PS3SCALE_PATH_MAX];
    int written;

    if (g_ps2_config_download_running || g_ps2_config_download_done ||
        g_ps2_config_download_thread_started) {
        set_details_message("WAIT FOR THE CURRENT PS2 CONFIG TASK TO FINISH.");
        return;
    }
    if (!game || !entry) {
        set_details_message("NO VERIFIED PS2 CONFIG MATCHES THIS GAME ID.");
        return;
    }
    if (ps3scale_ps2_config_build_url(entry, url, sizeof(url)) != 0) {
        set_details_message("PS2 CONFIG SOURCE URL IS INVALID.");
        return;
    }

    if (strcmp(entry->type, "ENC") == 0) {
        if (game->kind != PS3SCALE_GAME_PS2_CLASSIC) {
            set_details_message("ENC CONFIGS ARE ONLY FOR PS2 CLASSICS.");
            return;
        }
        if (stat(PS3SCALE_PS2_CLASSICS_CONFIG_DIR, &st) != 0 ||
            !S_ISDIR(st.st_mode)) {
            set_details_message("INSTALL THE WEBMAN PS2CONFIG PACKAGE FOR ENC FILES.");
            return;
        }
        written = snprintf(target, sizeof(target), "%s/%s.ENC",
                           PS3SCALE_PS2_CLASSICS_CONFIG_DIR, entry->title_id);
    } else {
        if (game->kind != PS3SCALE_GAME_PS2_ISO) {
            set_details_message("THIS CONFIG TYPE REQUIRES A STANDARD PS2 ISO.");
            return;
        }
        written = snprintf(target, sizeof(target), "%s.CONFIG", game->path);
    }
    if (written < 0 || (size_t)written >= sizeof(target)) {
        set_details_message("PS2 CONFIG TARGET PATH IS TOO LONG.");
        return;
    }
    if (stat(target, &st) == 0) {
        set_details_message("CONFIG ALREADY EXISTS. IT WAS NOT OVERWRITTEN.");
        return;
    }

    written = snprintf(temporary, sizeof(temporary), "%s.ps3scale.tmp", target);
    if (written < 0 || (size_t)written >= sizeof(temporary)) {
        set_details_message("PS2 CONFIG TEMPORARY PATH IS TOO LONG.");
        return;
    }

    snprintf(g_ps2_config_download_url, sizeof(g_ps2_config_download_url),
             "%s", url);
    snprintf(g_ps2_config_download_path, sizeof(g_ps2_config_download_path),
             "%s", target);
    snprintf(g_ps2_config_download_temp_path,
             sizeof(g_ps2_config_download_temp_path), "%s", temporary);
    snprintf(g_ps2_config_download_type, sizeof(g_ps2_config_download_type),
             "%s", entry->type);
    g_ps2_config_download_result = -1;
    g_ps2_config_download_done = 0;
    g_ps2_config_download_running = 1;

    if (sysThreadCreate(&g_ps2_config_download_thread,
                        ps2_config_download_worker, NULL,
                        1001, 64 * 1024, 0,
                        "ps3scale_ps2cfg") != 0) {
        g_ps2_config_download_running = 0;
        set_details_message("COULD NOT START PS2 CONFIG DOWNLOAD WORKER.");
        return;
    }
    g_ps2_config_download_thread_started = 1;
    set_details_message("DOWNLOADING VERIFIED PS2 CONFIG. ORIGINAL GAME FILE IS UNCHANGED.");
}

static void service_ps2_config_download(void)
{
    if (!g_ps2_config_download_done)
        return;

    if (g_ps2_config_download_thread_started) {
        u64 thread_result = 0;
        (void)sysThreadJoin(g_ps2_config_download_thread, &thread_result);
        g_ps2_config_download_thread_started = 0;
    }

    if (g_ps2_config_download_result == 0)
        set_details_message("PS2 CONFIG SAVED SEPARATELY. ORIGINAL GAME FILE IS UNCHANGED.");
    else if (g_ps2_config_download_result == -2)
        set_details_message("CONFIG ALREADY EXISTS. IT WAS NOT OVERWRITTEN.");
    else if (g_ps2_config_download_result == -4)
        set_details_message("DOWNLOADED CONFIG FAILED VALIDATION. FILE REMOVED.");
    else if (g_ps2_config_download_result == -5)
        set_details_message("COULD NOT SAVE CONFIG. CHECK THE GAME FOLDER.");
    else
        set_details_message("PS2 CONFIG DOWNLOAD FAILED. GAME FILES WERE NOT CHANGED.");

    g_ps2_config_download_done = 0;
}

static int ps2_config_target_exists(const ps3scale_game_t *game,
                                    const ps3scale_ps2_config_entry_t *entry)
{
    char target[PS3SCALE_PATH_MAX];
    struct stat st;
    int written;

    if (!game || !entry)
        return 0;
    if (strcmp(entry->type, "ENC") == 0)
        written = snprintf(target, sizeof(target), "%s/%s.ENC",
                           PS3SCALE_PS2_CLASSICS_CONFIG_DIR, entry->title_id);
    else
        written = snprintf(target, sizeof(target), "%s.CONFIG", game->path);
    if (written < 0 || (size_t)written >= sizeof(target))
        return 0;
    return stat(target, &st) == 0;
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

static void draw_splash(void)
{
    unsigned alpha = 255u;
    int frame = g_splash_frames;
    float progress = (float)(frame % 180) / 180.0f;

    if (frame < 24) alpha = (unsigned)(frame * 255 / 24);
    if (alpha > 255u) alpha = 255u;

    set_ui_font_size(34, 50);
    draw_text(246.0f, 82.0f,
              color_with_alpha(0x20263aff, alpha), "PS3");
    draw_text(366.0f, 82.0f,
              color_with_alpha(0x875be8ff, alpha), "SCALE");
    set_ui_font_size(12, 18);
    draw_text(234.0f, 146.0f,
              color_with_alpha(0x514b69ff, alpha),
              "PS3 & PS2 Enhancement Management");

    set_ui_font_size(11, 17);
    draw_text(352.0f, 204.0f,
              color_with_alpha(0x514b69ff, alpha), "Special Thanks");
    draw_solid_rect(250.0f, 213.0f, 88.0f, 2.0f,
                    color_with_alpha(0xb9a7e8ff, alpha));
    draw_solid_rect(516.0f, 213.0f, 88.0f, 2.0f,
                    color_with_alpha(0xb9a7e8ff, alpha));

    /* Artemis remains a simple vector mark; Apollo uses the user's new logo. */
    draw_circle_outline(312.0f, 278.0f, 25.0f,
                        color_with_alpha(0x875be8ff, alpha));
    draw_icon_line(292.0f, 298.0f, 312.0f, 244.0f,
                   color_with_alpha(0x875be8ff, alpha));
    draw_icon_line(312.0f, 244.0f, 332.0f, 298.0f,
                   color_with_alpha(0x875be8ff, alpha));
    draw_icon_line(300.0f, 282.0f, 324.0f, 282.0f,
                   color_with_alpha(0x875be8ff, alpha));
    set_ui_font_size(10, 15);
    draw_text(268.0f, 322.0f,
              color_with_alpha(0x20263aff, alpha), "A R T E M I S");

    draw_icon_line(426.0f, 248.0f, 426.0f, 342.0f,
                   color_with_alpha(0xb9a7e8ff, alpha));
    if (g_apollo_texture) {
        ya2d_drawTextureEx(g_apollo_texture, 470.0f, 242.0f,
                           0.0f, 88.0f, 88.0f);
        draw_solid_rect(470.0f, 242.0f, 88.0f, 88.0f,
                        color_with_alpha(0xfaf8ffff, 255u - alpha));
    }
    set_ui_font_size(10, 15);
    draw_text(566.0f, 286.0f,
              color_with_alpha(0x20263aff, alpha), "APOLLO SAVE");

    draw_solid_rect(202.0f, 390.0f, 450.0f, 12.0f, 0xe2dcf3ff);
    draw_outline_rect(202.0f, 390.0f, 450.0f, 12.0f, 0xd4c9edff);
    draw_solid_rect(205.0f, 393.0f, 444.0f * progress, 6.0f,
                    color_with_alpha(0x875be8ff, alpha));
    set_ui_font_size(10, 15);
    draw_text(342.0f, 421.0f,
              color_with_alpha(0x514b69ff, alpha),
              "Initializing PS3 SCALE ...");
    set_ui_font_size(14, 22);
}

static void draw_boot(void)
{
    const char *stage_text[] = {
        "Initializing graphics and controller",
        "Checking settings and audio",
        "SCANNING LOCAL GAME LIBRARY",
        "Checking companion tools",
        "Ready"
    };
    const int progress[] = {12, 28, 58, 88, 100};
    float width = (560.0f * (float)progress[g_boot_stage]) / 100.0f;

    draw_solid_rect(95.0f, 123.0f, 670.0f, 260.0f, 0x8d7ca32a);
    draw_solid_rect(92.0f, 118.0f, 670.0f, 260.0f, 0xfaf8ffff);
    draw_outline_rect(92.0f, 118.0f, 670.0f, 260.0f, 0xd9d1eaff);
    set_ui_font_size(26, 39);
    draw_text(132.0f, 150.0f, 0xffffffff, "PS3");
    draw_text(225.0f, 150.0f, 0xcaff39ff, "SCALE");
    set_ui_font_size(11, 17);
    draw_text(132.0f, 206.0f, 0x514b69ff,
              "Starting your game library");
    draw_solid_rect(132.0f, 270.0f, 590.0f, 16.0f, 0xe2dcf3ff);
    draw_solid_rect(136.0f, 274.0f, width, 8.0f, 0x875be8ff);
    if (g_boot_stage == 2) {
        float moving = 136.0f + (float)((g_frame_counter * 5u) % 500u);
        draw_solid_rect(moving, 274.0f, 62.0f, 8.0f, 0xc5aaffff);
    }
    set_ui_font_size(9, 14);
    draw_text(132.0f, 316.0f, 0x514b69ff, stage_text[g_boot_stage]);
    draw_text(132.0f, 346.0f,
              g_app_files_ok ? 0x9ee56aff : 0xffcc66ff,
              g_app_files_ok ? "APP FILE CHECK: OK" :
                               "APP FILE CHECK: VERIFY INSTALL");
    set_ui_font_size(14, 22);
}

static void draw_keyboard(void)
{
    int i;
    set_ui_font_size(12, 18);
    draw_text(34, 22, 0xcaff39ff, "PS3 SCALE");
    draw_text(34, 56, 0xffffffff,
              g_keyboard_support_mode ?
              (g_support_selected == 0 ? "REPORT TITLE" : "REPORT DETAILS") :
              "SEARCH GAMES");
    draw_text(34, 88, 0x9aa6adff,
              g_keyboard_support_mode ? "ENTER TEXT FOR YOUR SUPPORT REPORT" :
                                        "TYPE TO FILTER THE LIBRARY");
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


static void draw_soft_card(float x, float y, float w, float h, int selected)
{
    draw_solid_rect(x + 4.0f, y + 7.0f, w, h,
                    selected ? 0x665b8f50 : 0x5e718f35);
    draw_solid_rect(x, y, w, h,
                    selected ? 0xf4efffe8 : 0xfaf8ffe8);
    draw_outline_rect(x, y, w, h,
                      selected ? 0x9568edff : 0xd9d1eaff);
    draw_solid_rect(x + 8.0f, y + 5.0f, w - 16.0f, 1.5f, 0xffffffb8);
    if (selected) {
        draw_outline_rect(x - 2.0f, y - 2.0f, w + 4.0f, h + 4.0f,
                          0xb99aff70);
        draw_solid_rect(x + 10.0f, y + h - 5.0f,
                        w - 20.0f, 3.0f, 0x875be8ff);
    }
}

static void draw_text_centered(float x, float y, float w,
                               u32 color, const char *value)
{
    float text_width = (float)strlen(value ? value : "") *
                       (float)g_ui_font_width;
    if (text_width > w) text_width = w;
    draw_text(x + (w - text_width) * 0.5f, y, color, value);
}

static void draw_soft_chip(float x, float y, float w, const char *label,
                           int selected)
{
    draw_solid_rect(x, y, w, 24.0f,
                    selected ? 0xeee7ffff : 0xf8f6ffff);
    draw_outline_rect(x, y, w, 24.0f,
                      selected ? 0x9568edff : 0xd9d1eaff);
    set_ui_font_size(7, 11);
    draw_text_centered(x + 4.0f, y + 7.0f, w - 8.0f,
                       selected ? 0xcaff39ff : 0xffffffff, label);
}

static void draw_page_header(const char *title, const char *subtitle)
{
    draw_ps_symbol(22.0f, 11.0f, 0x29243fff);
    set_ui_font_size(15, 23);
    draw_text(68.0f, 16.0f, 0xffffffff, "PS3 SCALE");
    set_ui_font_size(10, 15);
    draw_text(218.0f, 22.0f, 0x875be8ff, title);
    set_ui_font_size(7, 11);
    draw_text(26.0f, 49.0f, 0x77718fff, subtitle);
    draw_solid_rect(24.0f, 68.0f, 202.0f, 2.0f, 0xb79affff);
    draw_solid_rect(24.0f, 70.0f, 202.0f, 1.0f, 0xffffffb8);
}

static void draw_soft_button(float x, float y, float w, float h,
                             const char *label, int selected)
{
    draw_soft_card(x, y, w, h, selected);
    set_ui_font_size(8, 12);
    draw_text_centered(x + 5.0f, y + (h - 12.0f) * 0.5f, w - 10.0f,
                       selected ? 0xcaff39ff : 0xffffffff, label);
}

static void draw_home(void)
{
    static const char *items[5] = {
        "Scan Games", "Online DB", "Settings", "Help", "About"
    };
    static const char *descriptions[5] = {
        "Detect PS3 & PS2 Titles", "Community Updates", "Customize Application",
        "Usage Guide & Support", "Credits & Information"
    };
    char line[192];
    int i;

    draw_ps_symbol(25.0f, 17.0f, 0x29243fff);
    set_ui_font_size(25, 37);
    draw_text(70.0f, 18.0f, 0xffffffff, "PS3");
    draw_text(148.0f, 18.0f, 0x875be8ff, "SCALE");
    set_ui_font_size(8, 12);
    draw_text(72.0f, 61.0f, 0x514b69ff,
              "PS3 & PS2 ENHANCEMENT MANAGEMENT");
    draw_solid_rect(72.0f, 82.0f, 252.0f, 2.0f, 0xb69affff);
    draw_text(72.0f, 98.0f, 0x77718fff,
              "EXPLORE  |  ENHANCE  |  PLAY");

    draw_soft_card(568.0f, 18.0f, 116.0f, 32.0f, 0);
    draw_text(582.0f, 28.0f, 0x875be8ff, "CFW 4.92");
    draw_soft_card(692.0f, 18.0f, 148.0f, 32.0f, 0);
    draw_text(705.0f, 28.0f, 0x514b69ff, "PS3 SLIM (CECH-25xx)");

    draw_text(30.0f, 132.0f, 0x77718fff, "FEATURES");
    for (i = 0; i < 5; i++) {
        float x = 168.0f + (float)i * 134.0f;
        float y = 126.0f;
        const float w = 126.0f, h = 150.0f;
        int selected = (i == g_home_selected);
        draw_soft_card(x, y, w, h, selected);
        draw_menu_icon(i == 4 ? 5 : i, x + 42.0f, y + 16.0f, selected);
        set_ui_font_size(8, 12);
        draw_text_centered(x + 5.0f, y + 77.0f, w - 10.0f,
                           selected ? 0x875be8ff : 0x29243fff, items[i]);
        set_ui_font_size(6, 9);
        draw_text_centered(x + 6.0f, y + 103.0f, w - 12.0f,
                           0x77718fff, descriptions[i]);
        if (selected) {
            draw_circle_outline(x + w * 0.5f, y + 132.0f, 4.0f,
                                0x9568edff);
            draw_circle_outline(x + w * 0.5f, y + 132.0f, 7.0f,
                                0xb99aff70);
        }
    }

    snprintf(line, sizeof(line), "PS3  %u     PS2  %u     TOTAL  %u",
             (unsigned)library_total_count(LIBRARY_PS3),
             (unsigned)library_total_count(LIBRARY_PS2),
             (unsigned)(library_total_count(LIBRARY_PS3) +
                        library_total_count(LIBRARY_PS2)));
    draw_soft_card(30.0f, 410.0f, 322.0f, 44.0f, 0);
    set_ui_font_size(8, 12);
    draw_text(46.0f, 426.0f, 0x514b69ff, line);
    draw_text(31.0f, 480.0f, 0x77718fff,
              "D-PAD LEFT / RIGHT: MOVE   |   CROSS: SELECT   |   CIRCLE: EXIT");
    draw_text(31.0f, 496.0f, 0x875be8ff,
              "SELECT: COMPANION TOOLS");
    set_ui_font_size(14, 22);
}

static void draw_online_database(void)
{
    const ps3scale_profile_t *selected_profile = NULL;
    const ps3scale_community_file_t *selected_community = NULL;
    const ps3scale_ps2_config_entry_t *selected_ps2_config = NULL;
    const ps3scale_game_t *selected = NULL;
    char line[192];
    size_t i, visible_count = 0;

    draw_page_header("Online DB", "ONLINE GAME DATABASE | Profiles, community files and verified PS2 configurations");
    draw_soft_card(20.0f, 88.0f, 360.0f, 360.0f, 0);
    draw_soft_card(394.0f, 88.0f, 440.0f, 360.0f, 0);

    set_ui_font_size(8, 12);
    draw_text(38.0f, 104.0f, 0xffffffff, "Search updates");
    snprintf(line, sizeof(line), "QUERY: %.36s",
             g_online_query[0] ? g_online_query : "All local titles");
    draw_text(38.0f, 128.0f, 0x77718fff, line);
    snprintf(line, sizeof(line), "FILTER: %.26s", online_filter_label());
    draw_text(38.0f, 152.0f, 0xcaff39ff, line);

    selected = online_result_at((size_t)g_online_selected,
                                &selected_profile,
                                &selected_community,
                                &selected_ps2_config);

    for (i = 0; i < g_online_result_count; i++) {
        const ps3scale_profile_t *profile = NULL;
        const ps3scale_community_file_t *community = NULL;
        const ps3scale_ps2_config_entry_t *ps2_config = NULL;
        const ps3scale_game_t *game =
            online_result_at(i, &profile, &community, &ps2_config);
        const char *title;
        const char *kind;
        char row[128];
        float y;

        if (!game && !community && !ps2_config) continue;
        if (visible_count >= 8) break;
        if (ps2_config) {
            title = ps2_config->title;
            kind = "PS2 CFG";
        } else if (community) {
            title = community->title[0] ? community->title :
                    (game ? game->title : "GitHub Game File");
            kind = "GITHUB";
        } else {
            title = game ? game->title : "Game Profile";
            kind = "PROFILE";
        }
        y = 182.0f + (float)visible_count * 30.0f;
        draw_soft_card(32.0f, y, 334.0f, 26.0f,
                       (int)i == g_online_selected);
        draw_solid_rect(40.0f, y + 4.0f, 20.0f, 18.0f,
                        (i % 3 == 0) ? 0xe5dcf6ff :
                        (i % 3 == 1) ? 0xe0e7f4ff : 0xf0e3f2ff);
        draw_outline_rect(40.0f, y + 4.0f, 20.0f, 18.0f, 0xd2c6eaff);
        draw_solid_rect(44.0f, y + 8.0f, 12.0f, 2.0f, 0xb69affff);
        draw_solid_rect(44.0f, y + 13.0f, 8.0f, 2.0f, 0xcfc4e8ff);
        set_ui_font_size(6, 9);
        snprintf(row, sizeof(row), "%.30s", title);
        draw_text(68.0f, y + 4.0f,
                  (int)i == g_online_selected ? 0xcaff39ff : 0xffffffff, row);
        draw_text(302.0f, y + 4.0f, 0x77718fff, kind);
        visible_count++;
    }

    set_ui_font_size(8, 12);
    snprintf(line, sizeof(line), "RESULTS: %u",
             (unsigned)g_online_result_count);
    draw_text(38.0f, 422.0f, 0x77718fff, line);

    draw_text(414.0f, 104.0f, 0xffffffff, "Community (GitHub)");
    draw_text(414.0f, 128.0f, 0x77718fff,
              "Profiles, configs & enhancements");
    snprintf(line, sizeof(line), "CATALOG: %s",
             g_catalog_ready ? "READY" : "OFFLINE");
    draw_text(414.0f, 168.0f, 0x514b69ff, line);
    snprintf(line, sizeof(line), "GITHUB INDEX: %s",
             g_community_ready ? "READY" : "OFFLINE");
    draw_text(414.0f, 192.0f, 0x514b69ff, line);

    if (selected_profile && selected) {
        int exact_match = selected_community ?
            ps3scale_community_file_matches_exact_game(selected_community,
                                                        selected) : 1;
        draw_text(414.0f, 232.0f, 0xcaff39ff, "EXACT GAME PROFILE");
        snprintf(line, sizeof(line), "GAME: %.54s", selected->title);
        draw_text(414.0f, 260.0f, 0xffffffff, line);
        snprintf(line, sizeof(line), "TITLE ID: %.18s", selected->title_id);
        draw_text(414.0f, 286.0f, 0x514b69ff, line);
        snprintf(line, sizeof(line), "VERSION: %.18s", selected_profile->app_ver);
        draw_text(414.0f, 312.0f, 0x514b69ff, line);
        draw_text(414.0f, 342.0f,
                  exact_match ? 0x2c9f8eff : 0xffcc66ff,
                  exact_match ? "MATCH: EXACT TITLE ID / VERSION" :
                                "MATCH: REVIEW VERSION BEFORE INSTALL");
        draw_text(414.0f, 370.0f, 0x77718fff,
                  profile_for_game(selected) ?
                  "SAFE PROFILE: AVAILABLE" : "SAFE PROFILE: NOT AVAILABLE");
        draw_text(414.0f, 402.0f, 0x77718fff,
                  "APPLY: REVIEW + CONFIRM IN DETAILS");
    } else if (selected_ps2_config) {
        draw_text(414.0f, 232.0f, 0xcaff39ff, "PS2 COMMUNITY CONFIG");
        snprintf(line, sizeof(line), "GAME: %.42s", selected_ps2_config->title);
        draw_text(414.0f, 260.0f, 0xffffffff, line);
        snprintf(line, sizeof(line), "CONFIG TYPE: %.10s", selected_ps2_config->type);
        draw_text(414.0f, 286.0f, 0x514b69ff, line);
        snprintf(line, sizeof(line), "GAME ID: %.18s", selected_ps2_config->title_id);
        draw_text(414.0f, 312.0f, 0x514b69ff, line);
        draw_text(414.0f, 342.0f, 0xffcc66ff,
                  "APPLY: NOT A GRAPHICS PROFILE");
        draw_text(414.0f, 370.0f, 0x77718fff,
                  "Original ISO / classic files are never overwritten.");
    } else if (selected_community) {
        snprintf(line, sizeof(line), "GAME: %.42s", selected_community->title);
        draw_text(414.0f, 232.0f, 0xffffffff, line);
        snprintf(line, sizeof(line), "TITLE ID: %.18s", selected_community->title_id);
        draw_text(414.0f, 260.0f, 0x514b69ff, line);
        snprintf(line, sizeof(line), "CATEGORY: %.24s", selected_community->category);
        draw_text(414.0f, 286.0f, 0x514b69ff, line);
        snprintf(line, sizeof(line), "VERSION: %.18s",
                 selected_community->app_ver[0] ?
                 selected_community->app_ver : "Not specified");
        draw_text(414.0f, 312.0f, 0x514b69ff, line);
        snprintf(line, sizeof(line), "REGION: %s",
                 ps3scale_community_region_label(selected_community->title_id));
        draw_text(414.0f, 342.0f, 0x514b69ff, line);
        draw_text(414.0f, 370.0f, 0xffcc66ff,
                  "APPLY: REFERENCE ONLY - NOT INSTALLED");
    } else {
        draw_text(414.0f, 232.0f, 0x77718fff,
                  g_online_result_count ?
                  "Select a result to review its match." :
                  "No results for this search.");
        draw_text(414.0f, 260.0f, 0x77718fff,
                  "Try a game name, disc ID or title ID.");
    }

    if (g_online_message[0])
        draw_text(24.0f, 470.0f, 0xffcc66ff, g_online_message);
    set_ui_font_size(7, 11);
    draw_text(24.0f, 496.0f, 0x77718fff,
              "D-PAD SELECT   |   L1/R1 PREV/NEXT   |   TRIANGLE SEARCH   |   SQUARE FILTER   |   START SYNC   |   CIRCLE BACK");
    set_ui_font_size(14, 22);
}


static void draw_scan(void)
{
    char line[192];
    size_t count = library_count(g_library);
    size_t ps3_count = library_total_count(LIBRARY_PS3);
    size_t ps2_count = library_total_count(LIBRARY_PS2);
    size_t start = (g_selected > 7) ? (size_t)(g_selected - 7) : 0;
    size_t i;
    const ps3scale_game_t *selected;

    clamp_selection();
    selected = selected_game();
    draw_page_header("Game Library", "Browse your PS3 and PS2 collection");
    draw_soft_card(20.0f, 88.0f, 144.0f, 364.0f, 0);
    set_ui_font_size(8, 12);
    draw_text(34.0f, 104.0f, 0xffffffff, "LIBRARY");
    draw_soft_chip(30.0f, 130.0f, 124.0f, "All Games", 0);
    draw_soft_chip(30.0f, 162.0f, 124.0f, "PS3 Games", g_library == LIBRARY_PS3);
    draw_soft_chip(30.0f, 194.0f, 124.0f, "PS2 Games", g_library == LIBRARY_PS2);
    draw_soft_chip(30.0f, 226.0f, 124.0f, "Favorites", 0);
    draw_soft_chip(30.0f, 258.0f, 124.0f, "Recently Added", 0);
    set_ui_font_size(7, 11);
    snprintf(line, sizeof(line), "PS3  %u", (unsigned)ps3_count);
    draw_text(34.0f, 320.0f, 0x77718fff, line);
    snprintf(line, sizeof(line), "PS2  %u", (unsigned)ps2_count);
    draw_text(34.0f, 342.0f, 0x77718fff, line);
    snprintf(line, sizeof(line), "TOTAL  %u", (unsigned)(ps3_count + ps2_count));
    draw_text(34.0f, 364.0f, 0xcaff39ff, line);

    draw_soft_card(176.0f, 82.0f, 656.0f, 34.0f, 0);
    set_ui_font_size(8, 12);
    snprintf(line, sizeof(line), "Search games: %.44s",
             g_search[0] ? g_search : "All titles");
    draw_text(194.0f, 93.0f, 0x77718fff, line);
    draw_text(754.0f, 93.0f, 0xcaff39ff, "GRID");

    for (i = 0; i < 8 && start + i < count; i++) {
        const ps3scale_game_t *game = library_game_at(start + i);
        int is_selected = ((int)(start + i) == g_selected);
        float x = 176.0f + (float)(i % 4) * 94.0f;
        float y = 128.0f + (float)(i / 4) * 154.0f;
        u32 tile_color = (i % 3 == 0) ? 0xe9e1f8ff :
                         (i % 3 == 1) ? 0xe4e9f5ff : 0xf0e6f3ff;
        if (!game) continue;
        draw_soft_card(x, y, 86.0f, 146.0f, is_selected);
        draw_solid_rect(x + 6.0f, y + 7.0f, 74.0f, 91.0f, tile_color);
        if (is_selected && g_cover_texture) {
            ya2d_drawTextureEx(g_cover_texture, x + 6.0f, y + 7.0f,
                               0.0f, 74.0f, 91.0f);
        } else {
            draw_solid_rect(x + 12.0f, y + 16.0f, 62.0f, 3.0f, 0xb69affff);
            draw_solid_rect(x + 12.0f, y + 25.0f, 44.0f, 2.0f, 0xcfc4e8ff);
            draw_solid_rect(x + 12.0f, y + 76.0f, 54.0f, 2.0f, 0xcfc4e8ff);
            draw_solid_rect(x + 12.0f, y + 83.0f, 38.0f, 2.0f, 0xcfc4e8ff);
        }
        set_ui_font_size(6, 9);
        snprintf(line, sizeof(line), "%.12s", game->title);
        draw_text(x + 7.0f, y + 105.0f, 0xffffffff, line);
        draw_text(x + 7.0f, y + 120.0f,
                  0xcaff39ff,
                  (game->kind == PS3SCALE_GAME_PS2_ISO ||
                   game->kind == PS3SCALE_GAME_PS2_CLASSIC ||
                   game->kind == PS3SCALE_GAME_PS2_HDD) ? "PS2" : "PS3");
    }

    draw_soft_card(560.0f, 128.0f, 272.0f, 300.0f, 0);
    set_ui_font_size(9, 14);
    draw_text(578.0f, 146.0f, 0xffffffff, "Selected Game");
    if (selected) {
        snprintf(line, sizeof(line), "%.24s", selected->title);
        set_ui_font_size(10, 15);
        draw_text(578.0f, 176.0f, 0xcaff39ff, line);
        set_ui_font_size(7, 11);
        snprintf(line, sizeof(line), "TYPE: %.24s",
                 ps3scale_game_kind_name(selected->kind));
        draw_text(578.0f, 214.0f, 0x514b69ff, line);
        if (game_is_ps3(selected))
            snprintf(line, sizeof(line), "TITLE ID: %.18s", selected->title_id);
        else
            snprintf(line, sizeof(line), "PS2 SERIAL: %.18s",
                     selected->disc_id[0] ? selected->disc_id : selected->title_id);
        draw_text(578.0f, 238.0f, 0x514b69ff, line);
        snprintf(line, sizeof(line), "PATH: %.25s", selected->path);
        draw_text(578.0f, 262.0f, 0x77718fff, line);
        draw_text(578.0f, 302.0f, 0x2c9f8eff, "CROSS: OPEN DETAILS");
        draw_text(578.0f, 326.0f, 0x77718fff,
                  "Original game files stay untouched.");
        if (g_cover_texture)
            ya2d_drawTextureEx(g_cover_texture, 738.0f, 342.0f,
                               0.0f, 78.0f, 72.0f);
    } else {
        draw_text(578.0f, 184.0f, 0x77718fff, "No matching games found.");
        draw_text(578.0f, 210.0f, 0x77718fff, "Scan or change platform.");
    }

    if (g_games.truncated)
        draw_text(178.0f, 442.0f, 0xffcc66ff,
                  "WARNING: LIBRARY LIMIT REACHED (256 TITLES)");
    set_ui_font_size(7, 11);
    draw_text(24.0f, 478.0f,
              0x77718fff,
              "L1 / R1: PS3 / PS2   |   UP / DOWN: MOVE   |   CROSS: DETAILS");
    draw_text(24.0f, 496.0f, 0x875be8ff,
              "SELECT / START: SEARCH   |   SQUARE: RESCAN   |   CIRCLE: BACK");
    set_ui_font_size(14, 22);
}

static void draw_details(void)
{
    char line[192];
    const ps3scale_game_t *game = selected_game();
    const ps3scale_profile_t *profile = profile_for_game(game);
    const ps3scale_ps2_config_entry_t *ps2_config = ps2_config_for_game(game);
    int ps2_config_exists = ps2_config_target_exists(game, ps2_config);
    int script_state = profile_script_state_for_game(game, profile);

    draw_page_header("Game Details", "Review compatibility and available enhancements");
    if (!game) {
        draw_soft_card(24.0f, 92.0f, 806.0f, 340.0f, 0);
        draw_text(52.0f, 128.0f, 0xffffffff, "No game selected.");
        draw_text(52.0f, 474.0f, 0x77718fff, "CIRCLE: BACK");
        set_ui_font_size(14, 22);
        return;
    }

    draw_soft_card(20.0f, 92.0f, 154.0f, 292.0f, 0);
    if (g_cover_texture)
        ya2d_drawTextureEx(g_cover_texture, 34.0f, 108.0f,
                           0.0f, 126.0f, 174.0f);
    else {
        draw_solid_rect(34.0f, 108.0f, 126.0f, 174.0f, 0xe9e1f8ff);
        draw_text(50.0f, 184.0f, 0x77718fff, "NO COVER ART");
    }
    draw_soft_chip(38.0f, 300.0f, 118.0f,
                   game_is_ps3(game) ? "PS3" : "PS2", 1);
    draw_text(34.0f, 340.0f, 0x77718fff,
              "ORIGINAL FILES SAFE");

    draw_soft_card(186.0f, 92.0f, 326.0f, 292.0f, 0);
    set_ui_font_size(11, 17);
    snprintf(line, sizeof(line), "%.26s", game->title);
    draw_text(204.0f, 110.0f, 0xffffffff, line);
    set_ui_font_size(7, 11);
    snprintf(line, sizeof(line), "Platform: %s",
             game_is_ps3(game) ? "PlayStation 3" : "PlayStation 2");
    draw_text(204.0f, 150.0f, 0x514b69ff, line);
    snprintf(line, sizeof(line), "Game ID: %.18s",
             game->title_id[0] ? game->title_id :
             (game->disc_id[0] ? game->disc_id : "Not read"));
    draw_text(204.0f, 176.0f, 0x514b69ff, line);
    snprintf(line, sizeof(line), "Version: %.16s",
             game_is_ps3(game) ?
             (game->app_ver[0] ? game->app_ver : "Unknown - rescan") :
             "PS2 configuration");
    draw_text(204.0f, 202.0f, 0x514b69ff, line);
    snprintf(line, sizeof(line), "Location: %.25s", game->path);
    draw_text(204.0f, 228.0f, 0x77718fff, line);
    draw_text(204.0f, 270.0f, 0xffffffff, "Enhancement Status");
    if (game_is_ps3(game)) {
        if (profile) {
            snprintf(line, sizeof(line), "Profile: %.30s", profile->effect);
            draw_text(204.0f, 296.0f, 0x2c9f8eff, line);
            if (script_state == -2)
                draw_text(204.0f, 322.0f, 0xffcc66ff, "Profile script is stale.");
            else if (script_state == -1)
                draw_text(204.0f, 322.0f, 0xffcc66ff, "User script is protected.");
            else if (profile->patch_count != 1)
                draw_text(204.0f, 322.0f, 0xffcc66ff, "Manual-only profile.");
            else
                draw_text(204.0f, 322.0f, 0x2c9f8eff, "Exact profile available.");
        } else {
            /* NO PROFILE FOR THIS TITLE. GAME WILL START UNMODIFIED. */
            draw_text(204.0f, 296.0f, 0xffcc66ff,
                      "NO PROFILE FOR THIS TITLE.");
            draw_text(204.0f, 322.0f, 0x77718fff,
                      "GAME WILL START UNMODIFIED.");
        }
    } else if (ps2_config) {
        snprintf(line, sizeof(line), "Community config: %.10s", ps2_config->type);
        draw_text(204.0f, 296.0f, 0x2c9f8eff, line);
        draw_text(204.0f, 322.0f,
                  ps2_config_exists ? 0xffcc66ff : 0x2c9f8eff,
                  ps2_config_exists ? "Config already installed." :
                                      "Verified config available.");
    } else {
        draw_text(204.0f, 296.0f, 0xffcc66ff,
                  "No verified PS2 config matches this ID.");
        draw_text(204.0f, 322.0f, 0x77718fff,
                  "Original game image will not be modified.");
    }

    draw_soft_card(526.0f, 92.0f, 308.0f, 292.0f, 0);
    set_ui_font_size(9, 14);
    draw_text(544.0f, 108.0f, 0xffffffff, "Original vs Enhanced");
    draw_solid_rect(542.0f, 136.0f, 132.0f, 84.0f, 0xcbd8bfff);
    draw_solid_rect(680.0f, 136.0f, 136.0f, 84.0f, 0xc8d9c0ff);
    draw_solid_rect(542.0f, 136.0f, 132.0f, 28.0f, 0xa7c1a0ff);
    draw_solid_rect(680.0f, 136.0f, 136.0f, 28.0f, 0x9fc59fff);
    draw_solid_rect(542.0f, 190.0f, 132.0f, 30.0f, 0x8c9d7aff);
    draw_solid_rect(680.0f, 184.0f, 136.0f, 36.0f, 0x789c72ff);
    draw_icon_line(542.0f, 184.0f, 674.0f, 184.0f, 0xe9e5d8ff);
    draw_icon_line(680.0f, 178.0f, 816.0f, 178.0f, 0xf4f1e5ff);
    draw_solid_rect(674.0f, 136.0f, 6.0f, 84.0f, 0xfaf8ffff);
    draw_circle_outline(677.0f, 178.0f, 10.0f, 0x875be8ff);
    set_ui_font_size(6, 9);
    draw_text(554.0f, 228.0f, 0x514b69ff, "ORIGINAL");
    draw_text(744.0f, 228.0f, 0x875be8ff, "ENHANCED");
    draw_soft_chip(542.0f, 246.0f, 132.0f, "Enhancement Info", 1);
    draw_soft_chip(680.0f, 246.0f, 136.0f, "Compatibility", 0);
    if (profile || ps2_config) {
        draw_circle_outline(550.0f, 286.0f, 4.0f, 0x2c9f8eff);
        draw_text(562.0f, 282.0f, 0x514b69ff,
                  "Higher Resolution (profile-dependent)");
        draw_circle_outline(550.0f, 304.0f, 4.0f, 0x2c9f8eff);
        draw_text(562.0f, 300.0f, 0x514b69ff,
                  "Improved Texture Sharpness");
        draw_circle_outline(550.0f, 322.0f, 4.0f, 0x2c9f8eff);
        draw_text(562.0f, 318.0f, 0x514b69ff,
                  "Better Color & Lighting");
        draw_circle_outline(550.0f, 340.0f, 4.0f, 0x2c9f8eff);
        draw_text(562.0f, 336.0f, 0x514b69ff,
                  "Smoother Performance (game-specific)");
    } else {
        draw_text(544.0f, 292.0f, 0xffcc66ff,
                  "No verified profile is available.");
        draw_text(544.0f, 316.0f, 0x77718fff,
                  "This game will start unmodified.");
    }

    draw_soft_button(20.0f, 400.0f, 250.0f, 44.0f,
                     game_is_ps3(game) ?
                     (profile ? "TRIANGLE: APPLY PROFILE" : "NO VERIFIED PROFILE") :
                     (ps2_config ? "TRIANGLE: DOWNLOAD CONFIG" : "NO CONFIG MATCH"),
                     1);
    draw_soft_button(282.0f, 400.0f, 250.0f, 44.0f,
                     "SQUARE: PROFILE OPTIONS", 0);
    draw_soft_button(544.0f, 400.0f, 290.0f, 44.0f,
                     "CROSS: START GAME", 0);
    set_ui_font_size(7, 11);
    draw_text(24.0f, 474.0f, 0x77718fff,
              g_profile_confirm_pending ?
              "COMMUNITY PATCH: TRIANGLE CONFIRMS; CIRCLE CANCELS." :
              g_details_message);
    draw_text(24.0f, 496.0f, 0x875be8ff,
              "CROSS / START: PLAY   |   TRIANGLE: CONFIRM   |   CIRCLE: BACK");
    set_ui_font_size(14, 22);
}

static void draw_settings(void)
{
    static const char *labels[SETTINGS_COUNT] = {
        "Music", "Music Volume", "Animations (UI & Background)",
        "Check for USB Update", "Rescan Game Library",
        "Restore Default Settings", "Language", "Music Playlist"
    };
    static const char *sections[6] = {
        "General", "Display", "Audio", "Enhancements", "Network", "System"
    };
    char value[96];
    int i;

    draw_page_header("Settings", "Personalize PS3 SCALE; preferences are saved on the internal HDD");
    draw_soft_card(20.0f, 88.0f, 148.0f, 354.0f, 0);
    set_ui_font_size(8, 12);
    for (i = 0; i < 6; i++) {
        float y = 112.0f + (float)i * 46.0f;
        draw_soft_chip(28.0f, y, 132.0f, sections[i], i == 0);
    }

    draw_soft_card(184.0f, 88.0f, 650.0f, 354.0f, 0);
    set_ui_font_size(8, 12);
    draw_text(204.0f, 104.0f, 0xffffffff, "General Preferences");
    for (i = 0; i < SETTINGS_COUNT; i++) {
        float y = 132.0f + (float)i * 34.0f;
        int selected = (i == g_settings_selected);
        if (selected) {
            draw_solid_rect(198.0f, y - 5.0f, 620.0f, 29.0f, 0xeee8ffff);
            draw_outline_rect(198.0f, y - 5.0f, 620.0f, 29.0f, 0xbba5f0ff);
        }
        value[0] = 0;
        if (i == 0)
            snprintf(value, sizeof(value), "%s", g_music_enabled ? "ON" : "OFF");
        else if (i == 1)
            snprintf(value, sizeof(value), "%d%%", (g_music_volume * 100) / 255);
        else if (i == 2)
            snprintf(value, sizeof(value), "%s", g_animation_enabled ? "ON" : "OFF");
        else if (i == 3)
            snprintf(value, sizeof(value), "USB000 - USB007");
        else if (i == 4)
            snprintf(value, sizeof(value), "Current Library");
        else if (i == 5)
            snprintf(value, sizeof(value), "Defaults");
        else if (i == 6)
            snprintf(value, sizeof(value), "%s",
                     ps3scale_locale_language_name(g_language));
        else if (g_music_mode == 0)
            snprintf(value, sizeof(value), "AUTO: BIOS > GTA V");
        else
            snprintf(value, sizeof(value), "%s",
                     g_music_mode == 1 ? "BIOS ONLY" : "GTA V ONLY");

        set_ui_font_size(7, 11);
        draw_text(214.0f, y + 3.0f,
                  selected ? 0xcaff39ff : 0xffffffff, labels[i]);
        draw_text(604.0f, y + 3.0f,
                  selected ? 0xcaff39ff : 0x77718fff, value);
        if (i == 0 || i == 2) {
            draw_solid_rect(748.0f, y, 54.0f, 18.0f,
                            ((i == 0 && g_music_enabled) ||
                             (i == 2 && g_animation_enabled)) ?
                             0xded2ffff : 0xe7e3f0ff);
            draw_outline_rect(748.0f, y, 54.0f, 18.0f, 0xb9a7e8ff);
            draw_solid_rect(
                ((i == 0 && g_music_enabled) ||
                 (i == 2 && g_animation_enabled)) ? 780.0f : 752.0f,
                y + 3.0f, 12.0f, 12.0f, 0x875be8ff);
        } else if (i == 1) {
            float bar = (float)g_music_volume / 255.0f * 112.0f;
            draw_solid_rect(680.0f, y + 14.0f, 112.0f, 3.0f, 0xe0d9f0ff);
            draw_solid_rect(680.0f, y + 14.0f, bar, 3.0f, 0x875be8ff);
            draw_circle_outline(680.0f + bar, y + 15.5f, 5.0f, 0x875be8ff);
        }
    }

    set_ui_font_size(7, 11);
    if (g_music_track1_missing)
        draw_text(202.0f, 418.0f, 0xffcc66ff,
                  "MUSIC 1: BIOS - ZORROVIAN (USB IMPORT) | MUSIC 2: GTA V (BUILT IN).");
    else if (g_music1_imported_from_usb)
        draw_text(202.0f, 418.0f, 0x9ee56aff,
                  "MUSIC 1 IMPORTED FROM USB AND SAVED TO THE INTERNAL HDD.");
    else
        draw_text(202.0f, 418.0f, 0x77718fff, g_settings_message);

    draw_text(24.0f, 474.0f, 0x77718fff,
              "UP / DOWN: MOVE   |   CROSS: CHANGE   |   LEFT / RIGHT: ADJUST");
    draw_text(24.0f, 496.0f, 0x875be8ff,
              "CIRCLE: BACK");
    set_ui_font_size(14, 22);
}

static void draw_tools(void)
{
    char line[128];
    draw_page_header("Companion Tools", "Optional tools detected on the console");
    draw_soft_card(24.0f, 96.0f, 806.0f, 320.0f, 0);
    set_ui_font_size(10, 15);
    draw_text(48.0f, 120.0f, 0xffffffff, "APOLLO SAVE TOOL");
    snprintf(line, sizeof(line), "STATUS: %s",
             g_apollo_installed ? "INSTALLED" : "NOT DETECTED");
    draw_text(642.0f, 120.0f,
              g_apollo_installed ? 0x2c9f8eff : 0xffcc66ff, line);
    set_ui_font_size(7, 11);
    draw_text(48.0f, 148.0f, 0x77718fff,
              "Save backup, restore, resign, trophy and license tools.");
    draw_solid_rect(48.0f, 176.0f, 748.0f, 1.0f, 0xd9d1eaff);

    set_ui_font_size(10, 15);
    draw_text(48.0f, 196.0f, 0xffffffff, "ARTEMIS PS3");
    snprintf(line, sizeof(line), "STATUS: %s",
             g_artemis_installed ? "INSTALLED" : "NOT DETECTED");
    draw_text(642.0f, 196.0f,
              g_artemis_installed ? 0x2c9f8eff : 0xffcc66ff, line);
    set_ui_font_size(7, 11);
    draw_text(48.0f, 224.0f, 0x77718fff,
              "Title-specific cheat database and its own runtime plugin.");
    draw_solid_rect(48.0f, 252.0f, 748.0f, 1.0f, 0xd9d1eaff);

    set_ui_font_size(10, 15);
    draw_text(48.0f, 272.0f, 0xffffffff, "webMAN MOD");
    snprintf(line, sizeof(line), "STATUS: %s",
             g_webman_installed ? "PLUGIN FOUND" : "NOT DETECTED");
    draw_text(642.0f, 272.0f,
              g_webman_installed ? 0x2c9f8eff : 0xffcc66ff, line);
    set_ui_font_size(7, 11);
    draw_text(48.0f, 300.0f, 0x77718fff,
              "Game mounting, FTP, temperature and optional PS3MAPI.");
    draw_text(48.0f, 362.0f, 0x77718fff,
              "CROSS: REFRESH TOOL STATUS");
    draw_text(24.0f, 478.0f, 0x77718fff, "CIRCLE: BACK");
    set_ui_font_size(14, 22);
}

static void draw_help(void)
{
    static const char *step_titles[5] = {
        "Scan Games", "Select a Game", "Review Profile",
        "Apply Safely", "Start Game"
    };
    static const char *step_lines[5] = {
        "Find PS3 / PS2 titles.",
        "Choose a library game.",
        "Check version and match.",
        "Apply a verified profile.",
        "Start and enjoy your game."
    };
    static const char *actions[2] = { "Report an Issue", "Send Feedback" };
    int i;

    draw_page_header("Help & Support", "A quick step-by-step guide to PS3 SCALE");
    for (i = 0; i < 5; i++) {
        float x = 20.0f + (float)i * 164.0f;
        draw_soft_card(x, 112.0f, 154.0f, 202.0f, 0);
        set_ui_font_size(10, 15);
        draw_circle_outline(x + 28.0f, 144.0f, 13.0f, 0x9568edff);
        {
            char number[8];
            snprintf(number, sizeof(number), "%d", i + 1);
            draw_text(x + 24.0f, 139.0f, 0xcaff39ff, number);
        }
        draw_menu_icon(i, x + 98.0f, 126.0f, 0);
        set_ui_font_size(8, 12);
        draw_text(x + 12.0f, 176.0f, 0xffffffff, step_titles[i]);
        set_ui_font_size(5, 8);
        draw_text(x + 12.0f, 208.0f, 0x77718fff, step_lines[i]);
        draw_solid_rect(x + 12.0f, 268.0f, 130.0f, 2.0f, 0xd9d1eaff);
    }

    draw_soft_card(20.0f, 332.0f, 814.0f, 112.0f, 0);
    set_ui_font_size(9, 14);
    draw_text(38.0f, 346.0f, 0xffffffff, "Support & Contact");
    set_ui_font_size(7, 11);
    draw_text(38.0f, 376.0f, 0x77718fff, "support@ps3scale.app");
    draw_soft_button(252.0f, 362.0f, 220.0f, 42.0f,
                     actions[1], g_help_selected == 1);
    draw_soft_button(486.0f, 362.0f, 220.0f, 42.0f,
                     actions[0], g_help_selected == 0);
    draw_text(38.0f, 414.0f, 0x77718fff,
              "Reports include console model and firmware; saved locally before sending.");
    set_ui_font_size(7, 11);
    draw_text(24.0f, 478.0f, 0x77718fff,
              "UP / DOWN: CHOOSE SUPPORT ACTION   |   CROSS: OPEN   |   CIRCLE: BACK");
    set_ui_font_size(14, 22);
}

static void draw_support_form(void)
{
    int i;
    static const char *labels[3] = {
        "Report Title", "Problem Details", "Submit Report"
    };

    draw_page_header(g_support_is_feedback ? "Send Feedback" : "Report an Issue",
                     "Send a note from PS3 SCALE");
    draw_soft_card(24.0f, 96.0f, 806.0f, 330.0f, 0);
    for (i = 0; i < 3; i++) {
        float y = 118.0f + (float)i * 86.0f;
        int selected = (i == g_support_selected);
        draw_soft_card(42.0f, y, 768.0f, 68.0f, selected);
        set_ui_font_size(8, 12);
        draw_text(60.0f, y + 9.0f, 0xcaff39ff, labels[i]);
        set_ui_font_size(7, 11);
        if (i == 0)
            draw_text(60.0f, y + 38.0f, 0xffffffff,
                      g_support_title[0] ? g_support_title :
                                           "Press CROSS to enter a title");
        else if (i == 1)
            draw_text(60.0f, y + 38.0f, 0xffffffff,
                      g_support_details[0] ? g_support_details :
                                             "Press CROSS to describe the issue");
        else
            draw_text(60.0f, y + 38.0f, 0x77718fff,
                      "Save locally; send online if the service is configured");
    }
    set_ui_font_size(7, 11);
    draw_text(44.0f, 388.0f, 0x77718fff,
              "Device model, firmware and app version are added automatically.");
    draw_text(44.0f, 408.0f, 0x77718fff, g_support_status);
    draw_text(24.0f, 478.0f, 0x77718fff,
              "UP / DOWN: MOVE   |   CROSS: EDIT / SAVE   |   CIRCLE: BACK");
    set_ui_font_size(14, 22);
}

static int support_json_escape(char *destination, size_t capacity,
                               const char *source)
{
    size_t used = 0;
    if (!destination || capacity == 0 || !source)
        return -1;

    while (*source) {
        unsigned char c = (unsigned char)*source++;
        if (c == '"' || c == '\\') {
            if (used + 2 >= capacity) return -1;
            destination[used++] = '\\';
            destination[used++] = (char)c;
        } else if (c == '\n' || c == '\r') {
            if (used + 2 >= capacity) return -1;
            destination[used++] = '\\';
            destination[used++] = (c == '\n') ? 'n' : 'r';
        } else if (c < 0x20) {
            continue;
        } else {
            if (used + 1 >= capacity) return -1;
            destination[used++] = (char)c;
        }
    }
    destination[used] = '\0';
    return 0;
}

static int build_support_json(void)
{
    char title[128], details[128], model[128];
    char firmware[96], platform[96];
    int length;

    if (support_json_escape(title, sizeof(title), g_support_title) != 0 ||
        support_json_escape(details, sizeof(details), g_support_details) != 0)
        return -1;
    if (g_hardware_info_valid) {
        if (support_json_escape(model, sizeof(model),
                                g_hardware_info.model_series) != 0 ||
            support_json_escape(firmware, sizeof(firmware),
                                g_hardware_info.firmware_version) != 0 ||
            support_json_escape(platform, sizeof(platform),
                                g_hardware_info.platform_id) != 0)
            return -1;
    } else {
        snprintf(model, sizeof(model), "UNKNOWN");
        snprintf(firmware, sizeof(firmware), "UNKNOWN");
        snprintf(platform, sizeof(platform), "UNKNOWN");
    }

    length = snprintf(g_support_json, sizeof(g_support_json),
        "{\"kind\":\"%s\",\"title\":\"%s\",\"details\":\"%s\","
        "\"model\":\"%s\",\"firmware\":\"%s\",\"platform\":\"%s\","
        "\"app_version\":\"ALPHA 0.2\"}",
        g_support_is_feedback ? "feedback" : "issue",
        title, details, model, firmware, platform);
    return (length < 0 || (size_t)length >= sizeof(g_support_json)) ? -1 : 0;
}

static void support_submit_worker(void *arg)
{
    long response_code = 0;
    (void)arg;
    g_support_send_result = ps3scale_online_post_json(
        PS3SCALE_SUPPORT_URL, g_support_json, strlen(g_support_json),
        &response_code);
    g_support_send_done = 1;
    g_support_send_running = 0;
}

static void start_support_send(void)
{
    int rc;

    if (strstr(PS3SCALE_SUPPORT_URL, "YOUR_PROJECT") != NULL) {
        snprintf(g_support_status, sizeof(g_support_status),
                 "LOCAL SAVE OK; ONLINE OFF.");
        return;
    }
    if (g_catalog_sync_running || g_catalog_thread_started ||
        g_game_launch_running || g_game_launch_thread_started ||
        g_profile_arm_running || g_profile_arm_thread_started) {
        snprintf(g_support_status, sizeof(g_support_status),
                 "LOCAL SAVE OK; NETWORK BUSY.");
        return;
    }
    if (g_support_thread_started || g_support_send_running) {
        snprintf(g_support_status, sizeof(g_support_status),
                 "ANOTHER REPORT IS SENDING.");
        return;
    }
    if (build_support_json() != 0) {
        snprintf(g_support_status, sizeof(g_support_status),
                 "LOCAL SAVE OK; FORMAT ERROR.");
        return;
    }

    g_support_send_done = 0;
    g_support_send_result = -1;
    g_support_send_running = 1;
    rc = sysThreadCreate(&g_support_thread, support_submit_worker, NULL,
                         1400, 0x8000, THREAD_JOINABLE,
                         "PS3 SCALE Support");
    if (rc == 0) {
        g_support_thread_started = 1;
        snprintf(g_support_status, sizeof(g_support_status),
                 "REPORT QUEUED FOR ONLINE SEND.");
    } else {
        g_support_send_running = 0;
        snprintf(g_support_status, sizeof(g_support_status),
                 "LOCAL SAVE OK; SEND START FAILED.");
    }
}

static void service_support_send(void)
{
    if (!g_support_send_done)
        return;
    if (g_support_thread_started) {
        u64 thread_result = 0;
        (void)sysThreadJoin(g_support_thread, &thread_result);
        g_support_thread_started = 0;
    }
    if (g_support_send_result == 0)
        snprintf(g_support_status, sizeof(g_support_status),
                 "REPORT SENT ONLINE.");
    else
        snprintf(g_support_status, sizeof(g_support_status),
                 "LOCAL SAVE OK; SEND FAILED.");
    g_support_send_done = 0;
}

static int save_support_report(void)
{
    FILE *fp;
    long timestamp;

    if (!g_support_title[0] || !g_support_details[0]) {
        snprintf(g_support_status, sizeof(g_support_status),
                 "ADD TITLE + DETAILS.");
        return -1;
    }

    (void)mkdir(PS3SCALE_SUPPORT_DIR, 0777);
    fp = fopen(PS3SCALE_SUPPORT_OUTBOX_PATH, "a");
    if (!fp) {
        snprintf(g_support_status, sizeof(g_support_status),
                 "REPORT SAVE FAILED.");
        return -1;
    }

    timestamp = (long)time(NULL);
    fprintf(fp, "\n--- PS3 SCALE %s REPORT %ld ---\n",
            g_support_is_feedback ? "FEEDBACK" : "ISSUE", timestamp);
    fprintf(fp, "TITLE: %s\n", g_support_title);
    fprintf(fp, "DETAILS: %s\n", g_support_details);
    if (g_hardware_info_valid) {
        fprintf(fp, "MODEL: %s\n", g_hardware_info.model_series);
        fprintf(fp, "FIRMWARE: %s\n", g_hardware_info.firmware_version);
        fprintf(fp, "PLATFORM: %s\n", g_hardware_info.platform_id);
    } else {
        fprintf(fp, "MODEL: UNKNOWN\nFIRMWARE: UNKNOWN\n");
    }
    fprintf(fp, "APP VERSION: ALPHA 0.2\n");
    if (fclose(fp) != 0) {
        snprintf(g_support_status, sizeof(g_support_status),
                 "REPORT SAVE FAILED.");
        return -1;
    }

    snprintf(g_support_status, sizeof(g_support_status),
             "SAVED TO LOCAL OUTBOX.");
    start_support_send();
    return 0;
}

static void draw_about(void)
{
    draw_page_header("About", "Credits, hardware and safety information");
    draw_soft_card(24.0f, 92.0f, 806.0f, 340.0f, 0);
    set_ui_font_size(12, 18);
    draw_text(48.0f, 116.0f, 0xffffffff,
              "PS3 / PS2 Game Library & Enhancement Manager");
    set_ui_font_size(8, 12);
    draw_text(48.0f, 158.0f, 0x77718fff,
              "PS3 SCALE helps organize game libraries and review version-matched community profiles.");
    draw_text(48.0f, 188.0f, 0x77718fff,
              "Scanning is read-only. Original game files are never replaced by a scan.");
    draw_text(48.0f, 218.0f, 0x77718fff,
              "No universal upscaler or frame generation is claimed.");
    draw_text(48.0f, 260.0f, 0xffffffff, "Hardware Information");
    if (g_hardware_info_valid) {
        draw_text(48.0f, 292.0f, 0x514b69ff, "Detected model:");
        draw_text(190.0f, 292.0f, 0x514b69ff, g_hardware_info.model_series);
        draw_text(48.0f, 322.0f, 0x514b69ff, "Console type:");
        draw_text(190.0f, 322.0f, 0x514b69ff,
                  ps3scale_console_type_name(g_hardware_info.console_type));
        draw_text(48.0f, 352.0f, 0x514b69ff, "Firmware:");
        draw_text(190.0f, 352.0f, 0x514b69ff,
                  g_hardware_info.firmware_version);
        draw_text(48.0f, 382.0f, 0x514b69ff, "Platform ID:");
        draw_text(190.0f, 382.0f, 0x514b69ff,
                  g_hardware_info.platform_id);
    } else {
        draw_text(48.0f, 292.0f, 0xffcc66ff,
                  "Hardware information is unavailable.");
    }
    set_ui_font_size(7, 11);
    draw_text(48.0f, 412.0f, 0x77718fff,
              "Alpha 0.2  |  CFW 4.92  |  PS3 FAT / SLIM / SUPER SLIM");
    draw_text(24.0f, 478.0f, 0x77718fff, "CIRCLE: BACK");
    set_ui_font_size(14, 22);
}

static void draw_profile_toast(void)
{
    if (!strstr(g_details_message, "ONE-LAUNCH PROFILE ARMED"))
        return;
    draw_soft_card(568.0f, 16.0f, 264.0f, 48.0f, 1);
    draw_circle_outline(590.0f, 40.0f, 10.0f, 0x2c9f8eff);
    draw_icon_line(585.0f, 40.0f, 589.0f, 44.0f, 0x2c9f8eff);
    draw_icon_line(589.0f, 44.0f, 596.0f, 35.0f, 0x2c9f8eff);
    set_ui_font_size(8, 12);
    draw_text(610.0f, 28.0f, 0xffffffff, "Profile Ready");
    set_ui_font_size(6, 9);
    draw_text(610.0f, 44.0f, 0x77718fff, "Start the game to use it");
    set_ui_font_size(14, 22);
}

static void draw_ui(void)
{
    tiny3d_Clear(0xfffafbff, TINY3D_CLEAR_ALL);

    /* Transparent glyphs, glass cards and PNG covers need RSX alpha state. */
    tiny3d_AlphaTest(1, 0x10, TINY3D_ALPHA_FUNC_GEQUAL);
    tiny3d_BlendFunc(1,
        TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA |
        TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA,
        TINY3D_BLEND_FUNC_DST_RGB_ONE_MINUS_SRC_ALPHA |
        TINY3D_BLEND_FUNC_DST_ALPHA_ONE_MINUS_SRC_ALPHA,
        TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD);

    tiny3d_Project2D();
    draw_scene_background();

    /* The 2D gradient wrote depth; clear only Z before the 3D staircase. */
    if (g_animation_enabled) {
        tiny3d_Clear(0x00000000, TINY3D_CLEAR_ZBUFFER);
        tiny3d_AlphaTest(1, 0x10, TINY3D_ALPHA_FUNC_GEQUAL);
        tiny3d_BlendFunc(1,
            TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA |
            TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA,
            TINY3D_BLEND_FUNC_DST_RGB_ONE_MINUS_SRC_ALPHA |
            TINY3D_BLEND_FUNC_DST_ALPHA_ONE_MINUS_SRC_ALPHA,
            TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD);
        draw_home_ambient();
    }

    if (g_screen != SCREEN_HOME && g_screen != SCREEN_SPLASH &&
        g_screen != SCREEN_BOOT) {
        draw_solid_rect(10.0f, 8.0f, 834.0f, 496.0f, 0xfaf8ff48);
        draw_outline_rect(10.0f, 8.0f, 834.0f, 496.0f, 0xd9d1ea88);
    }

    switch (g_screen) {
        case SCREEN_SPLASH: draw_splash(); break;
        case SCREEN_LANGUAGE: draw_language_selection(); break;
        case SCREEN_BOOT: draw_boot(); break;
        case SCREEN_KEYBOARD: draw_keyboard(); break;
        case SCREEN_HOME: draw_home(); break;
        case SCREEN_SCAN: draw_scan(); break;
        case SCREEN_DETAILS: draw_details(); break;
        case SCREEN_ONLINE_DB: draw_online_database(); break;
        case SCREEN_SETTINGS: draw_settings(); break;
        case SCREEN_TOOLS: draw_tools(); break;
        case SCREEN_ABOUT: draw_about(); break;
        case SCREEN_HELP: draw_help(); break;
        case SCREEN_SUPPORT_FORM: draw_support_form(); break;
        default: draw_home(); break;
    }
    if (g_screen == SCREEN_DETAILS)
        draw_profile_toast();
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
            /* Keep boot/local game use offline; refresh when the user opens
             * Online Graphics Database instead of starting network I/O at boot. */
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
    if (g_keyboard_support_mode) {
        snprintf(g_search, sizeof(g_search), "%s", g_search_backup);
        g_keyboard_support_mode = 0;
        g_screen = SCREEN_SUPPORT_FORM;
        return;
    }
    if (g_keyboard_online_mode) {
        snprintf(g_search, sizeof(g_search), "%s", g_scan_search_stash);
        g_keyboard_online_mode = 0;
        g_screen = SCREEN_ONLINE_DB;
        return;
    }
    snprintf(g_search, sizeof(g_search), "%s", g_search_backup);
    g_selected = 0;
    g_screen = SCREEN_SCAN;
    clamp_selection();
}

static void keyboard_apply(void)
{
    if (g_keyboard_support_mode) {
        if (g_support_selected == 0)
            snprintf(g_support_title, sizeof(g_support_title), "%s", g_search);
        else
            snprintf(g_support_details, sizeof(g_support_details), "%s", g_search);
        snprintf(g_search, sizeof(g_search), "%s", g_search_backup);
        g_keyboard_support_mode = 0;
        g_screen = SCREEN_SUPPORT_FORM;
        return;
    }
    if (g_keyboard_online_mode) {
        snprintf(g_online_query, sizeof(g_online_query), "%s", g_search);
        snprintf(g_search, sizeof(g_search), "%s", g_scan_search_stash);
        g_keyboard_online_mode = 0;
        g_online_selected = 0;
        rebuild_online_results();
        snprintf(g_online_message, sizeof(g_online_message),
                 "SEARCH COMPLETE: %u MATCHES.", (unsigned)g_online_result_count);
        g_screen = SCREEN_ONLINE_DB;
        return;
    }
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
    if (g_music_mode == 0) {
        g_music_start_track = g_music_next_track;
        g_music_next_track = (g_music_start_track == 1) ? 2 : 1;
    } else {
        g_music_start_track = (g_music_mode == 1) ? 1 : 2;
    }
    (void)save_app_settings();
    load_catalog_cache();
    (void)ps3scale_ps2_config_db_load(PS3SCALE_PS2_CONFIG_INDEX_PATH,
                                     &g_ps2_config_db);
    g_hardware_info_valid =
        (ps3scale_hardware_detect(&g_hardware_info) == 0);

    if (!ya2d_init()) return 1;
    g_apollo_texture = ya2d_loadPNGfromFile(PS3SCALE_APOLLO_LOGO_PATH);
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

    /* Start the bundled theme only when the saved Music setting is ON. */
    if (g_music_enabled) {
        (void)initialize_audio_backend();
        if (g_music_track1_missing)
            set_settings_message("MUSIC 1 IS NOT INSTALLED. MUSIC 2 IS PLAYING AS FALLBACK.");
    }

    if (g_language == LANGUAGE_UNSET) {
        g_language_choice = LANGUAGE_ENGLISH;
        g_splash_next_screen = SCREEN_LANGUAGE;
    } else {
        g_language_choice = g_language;
        g_splash_next_screen = SCREEN_BOOT;
    }
    g_splash_frames = 0;
    g_screen = SCREEN_SPLASH;
    g_boot_stage = 0;
    g_boot_stage_frames = 0;

    while (!g_exit_requested) {
        u16 up = 0, down = 0, square = 0, cross = 0, circle = 0;
        u16 l1 = 0, r1 = 0, triangle = 0, select = 0;
        u16 left = 0, right = 0, start = 0;

        sysUtilCheckCallback();
        service_catalog_sync();
        service_support_send();
        service_game_launch();
        service_profile_arm();
        service_ps2_config_download();
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

        if (g_screen == SCREEN_SPLASH) {
            if ((cross && !old_cross) || (start && !old_start) ||
                ++g_splash_frames >= 180) {
                g_screen = g_splash_next_screen;
                g_boot_stage = 0;
                g_boot_stage_frames = 0;
            }
        } else if (g_screen == SCREEN_LANGUAGE) {
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
            if (left && !old_left)
                g_home_selected = (g_home_selected + 4) % 5;
            if (right && !old_right)
                g_home_selected = (g_home_selected + 1) % 5;
            if (select && !old_select) {
                g_screen = SCREEN_TOOLS;
                refresh_companion_tool_status();
            }
            if (cross && !old_cross) {
                if (g_home_selected == 0) {
                    g_screen = SCREEN_SCAN;
                    g_selected = 0;
                } else if (g_home_selected == 1) {
                    g_screen = SCREEN_ONLINE_DB;
                    g_online_selected = 0;
                    rebuild_online_results();
                    if (g_community_ready) {
                        snprintf(g_online_message, sizeof(g_online_message),
                                 "SEARCHED %u PS3 GAMES; FOUND %u GITHUB FILES.",
                                 (unsigned)g_online_games_searched,
                                 (unsigned)g_online_community_result_count);
                    } else {
                        snprintf(g_online_message, sizeof(g_online_message),
                                 "GITHUB INDEX NOT LOADED. PRESS START TO SYNC.");
                    }
                    if (catalog_cache_is_stale() ||
                        community_index_cache_is_stale())
                        start_catalog_sync_worker(0);
                } else if (g_home_selected == 2) {
                    g_screen = SCREEN_SETTINGS;
                } else if (g_home_selected == 3) {
                    g_screen = SCREEN_HELP;
                    g_help_selected = 0;
                } else {
                    g_screen = SCREEN_ABOUT;
                }
            }
            if (circle && !old_circle)
                g_exit_requested = 1;
        } else if (g_screen == SCREEN_HELP) {
            if (up && !old_up) g_help_selected = 0;
            if (down && !old_down) g_help_selected = 1;
            if (cross && !old_cross) {
                g_support_is_feedback = (g_help_selected == 1);
                g_support_selected = 0;
                g_support_title[0] = '\0';
                g_support_details[0] = '\0';
                snprintf(g_support_status, sizeof(g_support_status),
                         "ENTER TITLE + DETAILS.");
                g_screen = SCREEN_SUPPORT_FORM;
            }
            if (circle && !old_circle)
                g_screen = SCREEN_HOME;
        } else if (g_screen == SCREEN_SUPPORT_FORM) {
            if (up && !old_up)
                g_support_selected = (g_support_selected + 2) % 3;
            if (down && !old_down)
                g_support_selected = (g_support_selected + 1) % 3;
            if (cross && !old_cross) {
                if (g_support_selected == 0)
                    (void)open_support_keyboard(0);
                else if (g_support_selected == 1)
                    (void)open_support_keyboard(1);
                else
                    (void)save_support_report();
            }
            if (circle && !old_circle)
                g_screen = SCREEN_HELP;
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
            if (cross && !old_cross && selected_game()) {
                const ps3scale_profile_t *profile = profile_for_game(selected_game());
                g_profile_confirm_pending = 0;
                if (profile)
                    set_details_message("EXACT ONLINE PROFILE FOUND. REVIEW BEFORE INSTALL.");
                else if (selected_game() &&
                         (selected_game()->kind == PS3SCALE_GAME_PS2_ISO ||
                          selected_game()->kind == PS3SCALE_GAME_PS2_CLASSIC))
                    set_details_message(ps2_config_for_game(selected_game()) ?
                        "VERIFIED PS2 CONFIG FOUND. TRIANGLE TO DOWNLOAD." :
                        "NO VERIFIED PS2 CONFIG FOUND FOR THIS GAME ID.");
                else
                    set_details_message("NO EXACT ONLINE PROFILE FOR THIS GAME UPDATE.");
                g_screen = SCREEN_DETAILS;
            }
            if (circle && !old_circle)
                g_screen = SCREEN_HOME;
        } else if (g_screen == SCREEN_DETAILS) {
            const ps3scale_game_t *game = selected_game();
            const ps3scale_profile_t *profile = profile_for_game(game);
            if (square && !old_square && game && game_is_ps3(game)) {
                int state;
                if (g_profile_arm_running || g_profile_arm_done) {
                    set_details_message("WAIT FOR PROFILE ARMING TO FINISH.");
                    state = 99;
                } else {
                    state = profile_script_state_for_game(game, profile);
                }
                if (state == 99) {
                    /* The profile worker owns this script path until it finishes. */
                } else if (state == 1 || state == -2) {
                    if (ps3scale_profile_script_remove(game->title_id) == 0)
                        set_details_message("ONE-LAUNCH PROFILE SCRIPT REMOVED FROM WEBMAN.");
                    else
                        set_details_message("COULD NOT REMOVE THE PS3 SCALE PROFILE SCRIPT.");
                } else if (state == -1) {
                    set_details_message("A USER WEBMAN SCRIPT EXISTS. IT WAS NOT OVERWRITTEN.");
                } else if (!profile) {
                    set_details_message("NO EXACT ONLINE PROFILE FOR THIS GAME UPDATE.");
                } else if (strcmp(profile->trust, "B_COMMUNITY_MANUAL") == 0) {
                    set_details_message("MANUAL-ONLY PROFILE. ONE-LAUNCH APPLY IS NOT SUPPORTED.");
                } else if (profile->patch_count != 1) {
                    set_details_message("MULTI-WRITE PROFILE: AUTO APPLY DISABLED FOR SAFETY.");
                } else {
                    g_profile_confirm_pending = 1;
                    set_details_message("COMMUNITY PATCH. TRIANGLE TO ARM ONE-LAUNCH PROFILE.");
                }
            }
            if (triangle && !old_triangle) {
                if (g_profile_confirm_pending) {
                    confirm_profile_install();
                } else if (game &&
                           (game->kind == PS3SCALE_GAME_PS2_ISO ||
                            game->kind == PS3SCALE_GAME_PS2_CLASSIC)) {
                    if (ps2_config_for_game(game))
                        start_ps2_config_download();
                    else if (game->kind == PS3SCALE_GAME_PS2_ISO) {
                        g_config_preview = !g_config_preview;
                        set_details_message("NO VERIFIED CONFIG. THIS IS PREVIEW ONLY.");
                    } else {
                        set_details_message("NO VERIFIED ENC CONFIG FOR THIS TITLE ID.");
                    }
                }
            }
            if (((cross && !old_cross) || (start && !old_start)) &&
                !g_profile_confirm_pending) {
                if (game && game->kind == PS3SCALE_GAME_PS2_CLASSIC)
                    set_details_message("START THIS PS2 CLASSIC WITH ITS XMB LAUNCHER.");
                else
                    start_selected_game();
            }
            if (circle && !old_circle) {
                if (g_profile_confirm_pending) {
                    g_profile_confirm_pending = 0;
                    set_details_message("PROFILE INSTALL CANCELLED.");
                } else {
                    g_screen = SCREEN_SCAN;
                }
            }
        } else if (g_screen == SCREEN_ONLINE_DB) {
            size_t matches = g_online_result_count;
            /* Keep the standard D-pad selection; L1/R1 are optional shortcuts. */
            if (up && !old_up && matches)
                g_online_selected = g_online_selected > 0 ?
                    g_online_selected - 1 : (int)matches - 1;
            if (down && !old_down && matches)
                g_online_selected = (g_online_selected + 1) % (int)matches;
            if (l1 && !old_l1 && matches) {
                g_online_selected = g_online_selected > 0 ?
                    g_online_selected - 1 : (int)matches - 1;
            }
            if (r1 && !old_r1 && matches)
                g_online_selected = (g_online_selected + 1) % (int)matches;
            if (square && !old_square) {
                g_online_filter_mode =
                    (g_online_filter_mode + 1) % ONLINE_FILTER_MODE_COUNT;
                g_online_selected = 0;
                rebuild_online_results();
                snprintf(g_online_message, sizeof(g_online_message),
                         "FILTER: %.54s | %u RESULTS.",
                         online_filter_label(), (unsigned)g_online_result_count);
            }
            if (triangle && !old_triangle)
                (void)open_online_search_keyboard();
            if (cross && !old_cross)
                open_selected_online_match();
            if (start && !old_start)
                start_catalog_sync_worker(1);
            if (circle && !old_circle)
                g_screen = SCREEN_HOME;
        } else if (g_screen == SCREEN_SETTINGS) {
            if (up && !old_up)
                g_settings_selected = (g_settings_selected + SETTINGS_COUNT - 1) % SETTINGS_COUNT;
            if (down && !old_down)
                g_settings_selected = (g_settings_selected + 1) % SETTINGS_COUNT;

            if (cross && !old_cross) {
                if (g_settings_selected == 0) {
                    if (g_music_enabled) {
                        set_background_music_enabled(0);
                        shutdown_audio_backend();
                        set_settings_message("BACKGROUND MUSIC PAUSED.");
                    } else {
                        set_background_music_enabled(1);
                        if (initialize_audio_backend() == 0)
                            set_settings_message("BACKGROUND MUSIC STARTED.");
                        else
                            set_settings_message("MUSIC FILE NOT FOUND OR AUDIO INIT FAILED.");
                    }
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
                    g_music_mode = 0;
                    g_music_next_track = 1;
                    g_music_start_track = 1;
                    g_animation_enabled = 1;
                    g_language = LANGUAGE_ENGLISH;
                    g_language_choice = LANGUAGE_ENGLISH;
                    if (!g_music_playing && initialize_audio_backend() != 0)
                        set_settings_message("MUSIC FILE NOT FOUND OR AUDIO INIT FAILED.");
                    else
                        set_settings_message("SETTINGS RESTORED TO DEFAULTS.");
                    (void)save_app_settings();
                } else if (g_settings_selected == 6) {
                    g_language = (g_language + 1) % 3;
                    g_language_choice = g_language;
                    (void)save_app_settings();
                } else if (g_settings_selected == 7) {
                    g_music_mode = (g_music_mode + 1) % 3;
                    g_music_start_track = (g_music_mode == 1) ? 1 :
                                          (g_music_mode == 2) ? 2 :
                                          (g_music_current_track ? g_music_current_track :
                                                                   g_music_next_track);
                    if (g_music_playing) {
                        shutdown_audio_backend();
                        if (initialize_audio_backend() != 0)
                            set_settings_message("MUSIC RESTART FAILED. CHECK TRACK FILES.");
                    }
                    (void)save_app_settings();
                }
            }

            if (g_settings_selected == 6) {
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
            if (g_settings_selected == 7) {
                if (left && !old_left) {
                    g_music_mode = (g_music_mode + 2) % 3;
                    g_music_start_track = (g_music_mode == 1) ? 1 :
                                          (g_music_mode == 2) ? 2 :
                                          (g_music_current_track ? g_music_current_track :
                                                                   g_music_next_track);
                    if (g_music_playing) {
                        shutdown_audio_backend();
                        if (initialize_audio_backend() != 0)
                            set_settings_message("MUSIC RESTART FAILED. CHECK TRACK FILES.");
                    }
                    (void)save_app_settings();
                }
                if (right && !old_right) {
                    g_music_mode = (g_music_mode + 1) % 3;
                    g_music_start_track = (g_music_mode == 1) ? 1 :
                                          (g_music_mode == 2) ? 2 :
                                          (g_music_current_track ? g_music_current_track :
                                                                   g_music_next_track);
                    if (g_music_playing) {
                        shutdown_audio_backend();
                        if (initialize_audio_backend() != 0)
                            set_settings_message("MUSIC RESTART FAILED. CHECK TRACK FILES.");
                    }
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
    if (g_catalog_thread_started) {
        u64 thread_result = 0;
        (void)sysThreadJoin(g_catalog_thread, &thread_result);
        g_catalog_thread_started = 0;
    }
    if (g_support_thread_started) {
        u64 thread_result = 0;
        (void)sysThreadJoin(g_support_thread, &thread_result);
        g_support_thread_started = 0;
    }
    if (g_game_launch_thread_started) {
        u64 thread_result = 0;
        (void)sysThreadJoin(g_game_launch_thread, &thread_result);
        g_game_launch_thread_started = 0;
    }
    if (g_profile_arm_thread_started) {
        u64 thread_result = 0;
        (void)sysThreadJoin(g_profile_arm_thread, &thread_result);
        g_profile_arm_thread_started = 0;
    }
    if (g_ps2_config_download_thread_started) {
        u64 thread_result = 0;
        (void)sysThreadJoin(g_ps2_config_download_thread, &thread_result);
        g_ps2_config_download_thread_started = 0;
    }
    ps3scale_ps2_config_db_free(&g_ps2_config_db);
    shutdown_audio_backend();
    sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
    if (g_cover_texture) ya2d_freeTexture(g_cover_texture);
    g_cover_texture = NULL;
    if (g_apollo_texture) ya2d_freeTexture(g_apollo_texture);
    g_apollo_texture = NULL;
    if (pad_ready) ya2d_deinit();
    return 0;
}
