#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
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
#include "ps3scale/ps2_config_store.h"
#include "ps3scale/ps2_game_patch.h"
#include "ps3scale/iso9660.h"
#include "ps3scale/atomic_file.h"
#include "ps3scale/support_payload.h"
#include "ps3scale/locale.h"
#include "ps3scale/hardware_info.h"
#include "ps3scale/catalog.h"
#include "ps3scale/community.h"
#include "ps3scale/online.h"
#include "ps3scale/profile_apply.h"
#include "font8x12.h"
#include "font_bitmap_z.h"

enum {
    LIBRARY_ALL = 0,
    LIBRARY_PS3 = 1,
    LIBRARY_PS2 = 2,
    LIBRARY_FAVORITES = 3,
    LIBRARY_RECENT = 4
};

typedef enum app_screen {
    SCREEN_HOME = 0,
    SCREEN_SCAN,
    SCREEN_DETAILS,
    SCREEN_FPS_CONTROL,
    SCREEN_PS2_CONFIG,
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
/*
 * Validation scans can contain 256 full game records (~300 KiB). Keep this
 * scratch list in static storage instead of the main or 32 KiB worker stack.
 * Profile/game launch tasks serialize access to this buffer.
 */
static ps3scale_game_list_t g_validation_scan;
static int g_selected = 0;
static int g_library = LIBRARY_ALL;
static int g_scan_sidebar_selected = LIBRARY_ALL;
static int g_scan_focus_sidebar = 0;
static char g_scan_message[128] = "";
static char g_favorite_ids[256][32];
static size_t g_favorite_count = 0;
static int g_recent_indices[256];
static time_t g_recent_mtimes[256];
static size_t g_recent_count = 0;
#define PS3SCALE_SCAN_GRID_COUNT 8
static ya2d_Texture *g_scan_cover_textures[PS3SCALE_SCAN_GRID_COUNT];
static char g_scan_cover_paths[PS3SCALE_SCAN_GRID_COUNT][PS3SCALE_PATH_MAX];
static int g_scan_cover_page = -1;
static int g_scan_cover_library = -1;
static int g_config_preview = 0;
static int g_exit_requested = 0;
static int g_home_selected = 0;
static float g_home_card_focus[5] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
static float g_scan_card_focus[PS3SCALE_SCAN_GRID_COUNT] = {1.0f};
static int g_help_selected = 0;
static int g_support_selected = 0;
static int g_support_is_feedback = 0;
static int g_keyboard_support_mode = 0;
static char g_support_title[PS3SCALE_SUPPORT_TITLE_MAX + 1] = "";
static char g_support_details[PS3SCALE_SUPPORT_DETAILS_MAX + 1] = "";
static char g_support_status[128] = "LOCAL OUTBOX READY.";
static char g_support_request_id[PS3SCALE_SUPPORT_REQUEST_ID_LENGTH + 1] = "";
static char g_support_sending_request_id[PS3SCALE_SUPPORT_REQUEST_ID_LENGTH + 1] = "";
static char g_support_pending_path[512] = "";
static char g_support_sending_path[512] = "";
static unsigned long g_support_request_sequence = 0;
static int g_support_report_saved = 0;
static int g_support_send_success = 0;
static int g_support_json_ready = 0;
static sys_ppu_thread_t g_support_thread;
static volatile int g_support_send_running = 0;
static volatile int g_support_send_done = 0;
static volatile int g_support_send_result = -1;
static volatile long g_support_send_http_status = 0;
static int g_support_thread_started = 0;
static char g_support_json[8192];
static app_screen_t g_screen = SCREEN_HOME;
static app_screen_t g_splash_next_screen = SCREEN_BOOT;
static int g_splash_frames = 0;
#define PS3SCALE_TRANSITION_FRAMES 16u
/* Keep controller hints inside the TV-safe area on overscanning displays. */
#define PS3SCALE_UI_FOOTER_PRIMARY_Y 452.0f
#define PS3SCALE_UI_FOOTER_SECONDARY_Y 468.0f
#define PS3SCALE_UI_FOOTER_SINGLE_Y 460.0f
static app_screen_t g_transition_screen = SCREEN_SPLASH;
static unsigned int g_transition_frames = PS3SCALE_TRANSITION_FRAMES;
static ps3scale_ps2_config_db_t g_ps2_config_db;
static sys_ppu_thread_t g_ps2_config_download_thread;
static int g_ps2_config_download_thread_started = 0;
static volatile int g_ps2_config_download_running = 0;
static volatile size_t g_ps2_config_downloaded = 0;
static volatile size_t g_ps2_config_download_total = 0;
static volatile int g_ps2_config_download_done = 0;
static volatile int g_ps2_config_download_result = 0;
static char g_ps2_config_download_url[512];
static char g_ps2_config_download_path[PS3SCALE_PATH_MAX];
static char g_ps2_config_download_temp_path[PS3SCALE_PATH_MAX];
static char g_ps2_config_download_backup_path[PS3SCALE_PATH_MAX],g_ps2_config_download_rollback_path[PS3SCALE_PATH_MAX];
static char g_ps2_config_download_type[PS3SCALE_PS2_CONFIG_TYPE_MAX];
static volatile int g_network_task_running = 0;
static volatile int g_community_download_running = 0;
static volatile size_t g_community_downloaded = 0;
static volatile size_t g_community_download_total = 0;
static volatile int g_community_download_done = 0;
static volatile int g_community_download_result = -1;
static int g_community_download_thread_started = 0;
static sys_ppu_thread_t g_community_download_thread;
static char g_community_download_url[1024];
static char g_community_download_path[PS3SCALE_PATH_MAX];
static char g_community_download_temp_path[PS3SCALE_PATH_MAX];
static int g_community_download_persistent = 0;
static volatile unsigned long g_community_download_expected_size = 0;
static char g_community_download_expected_sha1[41];
static char g_search[PS3SCALE_SUPPORT_DETAILS_MAX + 1] = "";
static u32 *g_font_texture = NULL;
static ya2d_Texture *g_cover_texture = NULL;
static ya2d_Texture *g_preview_before_texture = NULL;
static ya2d_Texture *g_preview_after_texture = NULL;
static char g_preview_key[32] = "";
static ya2d_Texture *g_apollo_texture = NULL;
static char g_cover_path[PS3SCALE_PATH_MAX] = "";

#define PS3SCALE_MUSIC_PATH "/dev_hdd0/game/PS3SCALE1/USRDIR/theme.mp3"
#define PS3SCALE_FAVORITES_PATH "/dev_hdd0/game/PS3SCALE1/USRDIR/favorites.txt"
#define PS3SCALE_MUSIC1_PATH "/dev_hdd0/game/PS3SCALE1/USRDIR/music1.mp3"
#define PS3SCALE_MUSIC2_PATH PS3SCALE_MUSIC_PATH
#define PS3SCALE_APOLLO_LOGO_PATH "/dev_hdd0/game/PS3SCALE1/USRDIR/apollo-save.png"
#define PS3SCALE_PREVIEW_DIR "/dev_hdd0/game/PS3SCALE1/USRDIR/previews"
#define PS3SCALE_COVER_MAX_BYTES (2u * 1024u * 1024u)
#define PS3SCALE_PREVIEW_MAX_BYTES (4u * 1024u * 1024u)
#define PS3SCALE_ARTEMIS_USERLIST_DIR "/dev_hdd0/game/ARTPS3001/USRDIR/USERLIST"
#define PS3SCALE_ARTEMIS_R5_USERLIST_DIR "/dev_hdd0/game/ARTZ00001/USRDIR/USERLIST"
#define PS3SCALE_ARTEMIS_EBOOT_PATH "/dev_hdd0/game/ARTPS3001/USRDIR/EBOOT.BIN"
#define PS3SCALE_ARTEMIS_R5_EBOOT_PATH "/dev_hdd0/game/ARTZ00001/USRDIR/EBOOT.BIN"
#define PS3SCALE_TEMP_ENHANCEMENT_LIST_PATH "/dev_hdd0/game/PS3SCALE1/USRDIR/temp_enhancements.lst"
#define PS3SCALE_SUPPORT_DIR "/dev_hdd0/game/PS3SCALE1/USRDIR/support"
#define PS3SCALE_SUPPORT_OUTBOX_DIR "/dev_hdd0/game/PS3SCALE1/USRDIR/support/outbox"
/* Replace with the URL of the separate PS3 SCALE Supabase project after deployment. */
#define PS3SCALE_SUPPORT_URL "https://YOUR_PROJECT.supabase.co/functions/v1/ps3scale-support"
#define PS3SCALE_SUPPORT_API_KEY "YOUR_PUBLISHABLE_KEY"
#define PS3SCALE_APP_VERSION "ALPHA 0.34"
#define PS3SCALE_PS2_CONFIG_INDEX_PATH "/dev_hdd0/game/PS3SCALE1/USRDIR/ps2_config_index.tsv"
#define PS3SCALE_PS2_CLASSICS_CONFIG_DIR "/dev_hdd0/game/PS2CONFIG/USRDIR/CONFIG/ENC"
#define PS3SCALE_PS2_CONFIG_DOWNLOAD_LIMIT (PS3SCALE_PS2_CONFIG_MAX_BYTES * 2u)
#define PS3SCALE_GAME_LAUNCH_ERROR_IDENTITY (-20)
#define PS3SCALE_GAME_LAUNCH_ERROR_SCRIPT (-21)

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
static volatile int g_music_mode = 0; /* 0=alternate both, 1=Music 1 only, 2=Music 2 only. */
static volatile int g_music_fade_ms = 1000;
static int g_music_next_track = 1;
static int g_music_start_track = 1;
static volatile int g_music_current_track = 0;
static int g_music_file_open = 0;
static volatile int g_music_track1_missing = 0;
static int g_music1_imported_from_usb = 0;
static int g_enhancement_persistent = 0;
static int g_settings_selected = 0;
static int g_animation_enabled = 1;
static unsigned long g_frame_counter = 0;
static char g_settings_message[128] = "SETTINGS ARE STORED ON THE PS3 HDD.";
static char g_details_message[128] = "NO VERIFIED GRAPHICS PROFILE FOR THIS VERSION.";
static int g_details_cache_valid = 0;
static int g_details_ps2_config_exists = 0;
static int g_ps2_config_manager_selection=0,g_ps2_config_manager_target_state=0,g_ps2_config_manager_backup_state=0,g_ps2_config_manager_rollback_state=0;
static int g_ps2_enhancer_guide_open=0;
static int g_ps2_patch_probe_valid=0;
static int g_ps2_patch_fps_available=0;
static int g_ps2_patch_widescreen_available=0;
static int g_ps2_patch_480p_available=0;
static char g_ps2_patch_serial[16] = "";
static uint32_t g_ps2_patch_elf_hash = 0;
static int g_ps2_patch_confirm_pending = 0;
static ps3scale_ps2_patch_kind_t g_ps2_patch_confirm_kind = PS3SCALE_PS2_PATCH_FPS;
static unsigned int g_ps2_enhancement_flags = PS3SCALE_PS2_CFG_ENABLE_L2H;
static size_t g_ps2_config_manager_target_size=0,g_ps2_config_manager_backup_size=0,g_ps2_config_manager_max_bytes=PS3SCALE_PS2_CONFIG_MAX_BYTES;
static char g_ps2_config_manager_message[128]="SELECT AN ACTION.";
static char g_ps2_config_manager_target[PS3SCALE_PATH_MAX],g_ps2_config_manager_backup[PS3SCALE_PATH_MAX];
static char g_ps2_config_manager_backup_temp[PS3SCALE_PATH_MAX],g_ps2_config_manager_rollback[PS3SCALE_PATH_MAX];
static char g_ps2_config_manager_restore_temp[PS3SCALE_PATH_MAX],g_ps2_config_manager_download_temp[PS3SCALE_PATH_MAX];
static int g_details_script_state = 0;
static int g_apollo_installed = 0;
static int g_artemis_installed = 0;
static int g_webman_installed = 0;
static int g_tools_selected = 0;
static char g_tools_message[128] = "SELECT A TOOL TO OPEN.";
static sys_ppu_thread_t g_tool_launch_thread;
static int g_tool_launch_thread_started = 0;
static volatile int g_tool_launch_running = 0;
static volatile int g_tool_launch_done = 0;
static volatile int g_tool_launch_result = -1;
static char g_tool_launch_title_id[10] = "";
#define SETTINGS_COUNT 10
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
static char g_search_backup[PS3SCALE_SUPPORT_DETAILS_MAX + 1] = "";
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
static int ps2_netemu_iso_supported(void);
static unsigned char g_extended_glyph_min_x[256];
static unsigned char g_extended_glyph_ink_width[256];
static int g_ui_font_width = 14;
static int g_ui_font_height = 22;

#define PS3SCALE_CATALOG_CACHE_PATH "/dev_hdd0/game/PS3SCALE1/USRDIR/graphics_catalog.tsv"
#define PS3SCALE_CATALOG_DOWNLOAD_PATH "/dev_hdd0/game/PS3SCALE1/USRDIR/graphics_catalog.download"
#define PS3SCALE_CATALOG_URL "https://raw.githubusercontent.com/Vikik0/PS3-SCALE/design/splash-transitions-ui-v6/PS3-SCALE-Alpha-0.2/data/online_profiles.tsv"
#define PS3SCALE_CATALOG_SYNC_SECONDS (24 * 60 * 60)
#define PS3SCALE_COMMUNITY_INDEX_CACHE_PATH "/dev_hdd0/game/PS3SCALE1/USRDIR/community_game_files.tsv"
#define PS3SCALE_COMMUNITY_INDEX_DOWNLOAD_PATH "/dev_hdd0/game/PS3SCALE1/USRDIR/community_game_files.download"
#define PS3SCALE_COMMUNITY_INDEX_URL "https://raw.githubusercontent.com/Vikik0/PS3-SCALE/design/splash-transitions-ui-v6/PS3-SCALE-Alpha-0.2/data/community_game_files.tsv"
#define PS3SCALE_COMMUNITY_INDEX_DOWNLOAD_LIMIT (4u * 1024u * 1024u)
#define PS3SCALE_MAX_ONLINE_RESULTS (PS3SCALE_COMMUNITY_MAX_FILES + PS3SCALE_MAX_GAMES + PS3SCALE_PS2_CONFIG_MAX_ENTRIES)
#define PS3SCALE_ONLINE_PAGE_SIZE 8u

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
static int g_online_focus_selection = 0;
static float g_online_row_focus = 1.0f;
enum {
    ONLINE_FILTER_GRAPHICS_EXACT = 0,
    ONLINE_FILTER_GRAPHICS_ANY = 1,
    ONLINE_FILTER_ALL_EXACT = 2,
    ONLINE_FILTER_ALL_ANY = 3,
    ONLINE_FILTER_MODE_COUNT = 4
};
static int g_online_filter_mode = ONLINE_FILTER_ALL_ANY;
static int g_online_first_opened = 0;
static char g_online_query[64] = "";
static char g_scan_search_stash[64] = "";
static int g_keyboard_online_mode = 0;
static int g_profile_confirm_pending = 0;
static int g_fps_control_selection = 0;
static char g_catalog_status[96] = "LOCAL PROFILE + GITHUB INDEX NOT LOADED";
static volatile size_t g_catalog_sync_downloaded = 0;
static volatile size_t g_catalog_sync_total = 0;
static volatile int g_catalog_sync_stage = 0;
static char g_online_message[128] = "SELECT A RESULT OR PRESS TRIANGLE TO REFRESH GITHUB.";
static void rebuild_online_results(void);
static volatile int g_game_launch_running = 0;
static volatile int g_game_launch_done = 0;
static volatile int g_game_launch_result = -1;
static int g_game_launch_profile_skipped = 0;
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
static ps3scale_game_t g_profile_arm_game_target;
static int g_auto_launch_after_profile_arm = 0;
static int g_force_unmodified_launch = 0;
static ps3scale_game_t g_auto_launch_target;

static void clamp_selection(void);
static void scan_library(void);
static int find_mounted_game(const ps3scale_game_t *selected,
                             ps3scale_game_t *mounted);
static int find_installed_game(const ps3scale_game_t *selected);
static void service_game_launch(void);
static void service_profile_arm(void);
static void start_selected_game(void);
static void refresh_details_cache(void);
static void service_ps2_config_download(void);
static void start_ps2_config_download(void);
static void refresh_ps2_config_manager_cache(void);
static void refresh_ps2_patch_availability(void);
static void perform_ps2_config_manager_action(void);
static void service_community_download(void);
static void start_community_file_download(const ps3scale_community_file_t *entry,
                                          const ps3scale_game_t *game);
static const ps3scale_ps2_config_entry_t *ps2_config_for_game(const ps3scale_game_t *game);
static const char *ps2_config_preferred_type(void);

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
    return 0;
}

static float music_sample_fade(off_t sample_position, off_t total_samples)
{
    float fade_in = 1.0f;
    float fade_out = 1.0f;
    off_t fade_frames = (off_t)g_music_fade_ms * 48;

    if (fade_frames < 12000)
        fade_frames = 12000;
    if (sample_position < 0)
        sample_position = 0;
    if (sample_position < fade_frames)
        fade_in = (float)sample_position / (float)fade_frames;

    if (total_samples > 0) {
        off_t remaining = total_samples - sample_position;
        if (remaining < fade_frames) {
            fade_out = remaining <= 0 ? 0.0f :
                       (float)remaining / (float)fade_frames;
        }
    }
    return fade_in < fade_out ? fade_in : fade_out;
}

static int cycle_music_fade_ms(int current, int direction)
{
    static const int choices[4] = {500, 1000, 1500, 2000};
    int index = 1;
    int i;
    for (i = 0; i < 4; i++) {
        if (choices[i] == current) {
            index = i;
            break;
        }
    }
    index = (index + (direction >= 0 ? 1 : 3)) % 4;
    return choices[index];
}

static void fill_audio_block(float *output)
{
    short pcm[AUDIO_BLOCK_SAMPLES * 2];
    size_t total = 0, target = sizeof(pcm), i, decoded_frames;
    off_t start_sample, track_length;
    float gain = g_music_enabled ? ((float)g_music_volume / 255.0f) : 0.0f;

    if (!output)
        return;
    if (gain <= 0.0f || !g_mpg123 || !g_music_file_open) {
        memset(output, 0, sizeof(float) * AUDIO_BLOCK_SAMPLES * 2);
        return;
    }

    memset(pcm, 0, sizeof(pcm));
    start_sample = mpg123_tell(g_mpg123);
    if (start_sample < 0)
        start_sample = 48000;
    track_length = mpg123_length(g_mpg123);

    while (total < target && g_audio_thread_running) {
        size_t decoded = 0;
        int rc = mpg123_read(g_mpg123, ((unsigned char *)pcm) + total,
                             target - total, &decoded);
        total += decoded;
        if (rc == MPG123_NEW_FORMAT) {
            long rate = 0; int channels = 0; int encoding = 0;
            if (mpg123_getformat(g_mpg123, &rate, &channels, &encoding) != MPG123_OK ||
                rate != 48000 || channels != MPG123_STEREO ||
                encoding != MPG123_ENC_SIGNED_16)
                break;
            continue;
        }
        if (rc == MPG123_DONE) {
            if (g_music_mode == 0) {
                int next_track = (g_music_current_track == 1) ? 2 : 1;
                if (open_music_track(next_track) != 0) {
                    if (next_track == 1)
                        g_music_track1_missing = 1;
                    if (open_music_track(2) != 0)
                        break;
                }
            } else if (mpg123_seek(g_mpg123, 0, SEEK_SET) < 0) {
                break;
            }
            /* Keep this block on one track; the next block fades in the next. */
            break;
        }
        if (rc == MPG123_ERR || (decoded == 0 && rc == MPG123_OK))
            break;
    }

    decoded_frames = total / (sizeof(short) * 2u);
    for (i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        float fade = 0.0f;
        if (i < decoded_frames)
            fade = music_sample_fade(start_sample + (off_t)i, track_length);
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

/* Tiny3D draws the icon shapes in vector geometry at the current video
   resolution, avoiding enlarged low-resolution bitmap icons. */
static void draw_icon_diamond(float cx, float cy,
                              float rx, float ry, u32 color)
{
    tiny3d_SetPolygon(TINY3D_QUADS);
    tiny3d_VertexPos(cx, cy - ry, 65535.0f);
    tiny3d_VertexColor(ui_theme_color(color));
    tiny3d_VertexPos(cx + rx, cy, 65535.0f);
    tiny3d_VertexPos(cx, cy + ry, 65535.0f);
    tiny3d_VertexPos(cx - rx, cy, 65535.0f);
    tiny3d_End();
}

static void draw_menu_icon(int index, float x, float y, int selected)
{
    u32 ink = selected ? 0xfffbffff : 0x32294fff;
    u32 detail = selected ? 0x6120bfff : 0xf6f1ffff;
    u32 tile = selected ? 0x833be3ff : 0xf7f5fdf4;
    u32 border = selected ? 0xb67affff : 0xe1d9eeff;

    /* Soft plate shadow, body and single top-edge specular highlight. */
    draw_solid_rect(x + 2.0f, y + 4.0f, 42.0f, 42.0f,
                    selected ? 0x50258c55 : 0x88809d32);
    draw_solid_rect(x, y, 42.0f, 42.0f, tile);
    draw_outline_rect(x, y, 42.0f, 42.0f, border);
    draw_solid_rect(x + 4.0f, y + 3.0f, 34.0f, 2.0f,
                    selected ? 0xb77bffb8 : 0xffffffc8);

    if (index == 0) {
        /* Filled, symmetric controller glyph with bright shoulder buttons. */
        draw_solid_rect(x + 8.0f, y + 14.0f, 26.0f, 15.0f, ink);
        draw_solid_rect(x + 6.0f, y + 18.0f, 6.0f, 13.0f, ink);
        draw_solid_rect(x + 30.0f, y + 18.0f, 6.0f, 13.0f, ink);
        draw_solid_rect(x + 9.0f, y + 12.0f, 10.0f, 3.0f, ink);
        draw_solid_rect(x + 23.0f, y + 12.0f, 10.0f, 3.0f, ink);
        draw_solid_rect(x + 13.0f, y + 19.0f, 3.0f, 11.0f, detail);
        draw_solid_rect(x + 9.0f, y + 23.0f, 11.0f, 3.0f, detail);
        draw_circle_outline(x + 28.0f, y + 20.5f, 2.2f, detail);
        draw_circle_outline(x + 32.0f, y + 25.5f, 2.2f, detail);
        draw_solid_rect(x + 20.0f, y + 18.0f, 3.0f, 2.0f,
                        selected ? 0xeccaffff : 0x9275c3ff);
    } else if (index == 1) {
        /* Three stacked 3D-looking layers, like the reference screenshot. */
        draw_icon_diamond(x + 21.0f, y + 12.0f, 15.0f, 7.0f, ink);
        draw_icon_diamond(x + 21.0f, y + 22.0f, 15.0f, 7.0f, ink);
        draw_icon_diamond(x + 21.0f, y + 32.0f, 15.0f, 7.0f, ink);
        draw_icon_line(x + 9.0f, y + 13.0f, x + 21.0f, y + 19.0f, detail);
        draw_icon_line(x + 21.0f, y + 19.0f, x + 33.0f, y + 13.0f, detail);
    } else if (index == 2) {
        /* Thick, high-contrast gear with a clear central bearing. */
        draw_gear_outline(x + 21.0f, y + 21.0f, 15.0f, ink);
        draw_gear_outline(x + 21.0f, y + 21.0f, 13.0f, ink);
        draw_circle_outline(x + 21.0f, y + 21.0f, 5.6f, ink);
        draw_circle_outline(x + 21.0f, y + 21.0f, 4.0f, detail);
    } else if (index == 3) {
        /* Help: legible question mark inside a double-ring circle. */
        draw_circle_outline(x + 21.0f, y + 21.0f, 15.0f, ink);
        draw_circle_outline(x + 21.0f, y + 21.0f, 13.0f, ink);
        draw_icon_line(x + 14.0f, y + 16.0f, x + 18.0f, y + 12.0f, ink);
        draw_icon_line(x + 18.0f, y + 12.0f, x + 24.0f, y + 12.0f, ink);
        draw_icon_line(x + 24.0f, y + 12.0f, x + 28.0f, y + 17.0f, ink);
        draw_icon_line(x + 28.0f, y + 17.0f, x + 21.0f, y + 24.0f, ink);
        draw_solid_rect(x + 20.0f, y + 23.0f, 3.0f, 5.0f, ink);
        draw_solid_rect(x + 20.0f, y + 31.0f, 3.0f, 3.0f, ink);
    } else if (index == 4) {
        /* Companion tools / enhancements. */
        draw_gear_outline(x + 21.0f, y + 21.0f, 14.0f, ink);
        draw_circle_outline(x + 21.0f, y + 21.0f, 5.0f, ink);
        draw_icon_line(x + 10.0f, y + 32.0f, x + 32.0f, y + 10.0f, ink);
    } else {
        /* About: information glyph matches the main dashboard reference. */
        draw_circle_outline(x + 21.0f, y + 21.0f, 15.0f, ink);
        draw_circle_outline(x + 21.0f, y + 21.0f, 13.0f, ink);
        draw_solid_rect(x + 19.5f, y + 18.0f, 3.0f, 12.0f, ink);
        draw_solid_rect(x + 19.5f, y + 12.0f, 3.0f, 3.0f, ink);
    }
}

static void draw_ps_symbol(float x, float y, u32 color)
{
    draw_icon_line(x + 5.0f, y + 2.0f, x + 5.0f, y + 32.0f, color);
    draw_icon_line(x + 5.0f, y + 2.0f, x + 21.0f, y + 7.0f, color);
    draw_icon_line(x + 21.0f, y + 7.0f, x + 21.0f, y + 24.0f, color);
    draw_icon_line(x + 21.0f, y + 24.0f, x + 5.0f, y + 19.0f, color);
    draw_icon_line(x + 5.0f, y + 19.0f, x + 27.0f, y + 31.0f, color);
    draw_icon_line(x + 27.0f, y + 31.0f, x + 38.0f, y + 26.0f, color);
    draw_icon_line(x + 38.0f, y + 26.0f, x + 38.0f, y + 22.0f, color);
    draw_icon_line(x + 38.0f, y + 22.0f, x + 27.0f, y + 18.0f, color);
    draw_icon_line(x + 27.0f, y + 18.0f, x + 16.0f, y + 22.0f, color);
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

static void favorite_identity(const ps3scale_game_t *game,
                              char *out, size_t capacity)
{
    uLong hash;
    if (!out || capacity == 0) return;
    out[0] = '\0';
    if (!game) return;
    if (game->title_id[0]) {
        snprintf(out, capacity, "T:%s", game->title_id);
    } else if (game->disc_id[0]) {
        snprintf(out, capacity, "D:%s", game->disc_id);
    } else {
        hash = crc32(0L, Z_NULL, 0);
        hash = crc32(hash, (const Bytef *)game->path,
                     (uInt)strlen(game->path));
        snprintf(out, capacity, "P:%08lX", (unsigned long)hash);
    }
}

static int favorite_key_index(const char *key)
{
    size_t i;
    for (i = 0; i < g_favorite_count; i++)
        if (strcmp(g_favorite_ids[i], key) == 0)
            return (int)i;
    return -1;
}

static int game_is_favorite(const ps3scale_game_t *game)
{
    char key[32];
    if (!game) return 0;
    favorite_identity(game, key, sizeof(key));
    return key[0] && favorite_key_index(key) >= 0;
}

static void load_favorites(void)
{
    FILE *fp = fopen(PS3SCALE_FAVORITES_PATH, "r");
    char line[64];
    g_favorite_count = 0;
    if (!fp) return;
    while (g_favorite_count < 256 && fgets(line, sizeof(line), fp)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = '\0';
        if (n > 0 && favorite_key_index(line) < 0) {
            snprintf(g_favorite_ids[g_favorite_count],
                     sizeof(g_favorite_ids[g_favorite_count]), "%s", line);
            g_favorite_count++;
        }
    }
    fclose(fp);
}

static int save_favorites(void)
{
    char contents[256u * 33u];
    size_t used = 0;
    size_t i;

    for (i = 0; i < g_favorite_count; i++) {
        int written = snprintf(contents + used, sizeof(contents) - used,
                               "%s\n", g_favorite_ids[i]);
        if (written < 0 || (size_t)written >= sizeof(contents) - used)
            return -1;
        used += (size_t)written;
    }
    return ps3scale_atomic_file_write(PS3SCALE_FAVORITES_PATH,
                                      contents, used);
}

static void toggle_favorite(const ps3scale_game_t *game)
{
    char key[32];
    int index;
    if (!game) return;
    favorite_identity(game, key, sizeof(key));
    if (!key[0]) return;
    index = favorite_key_index(key);
    if (index >= 0) {
        if ((size_t)index + 1 < g_favorite_count)
            memmove(g_favorite_ids[index], g_favorite_ids[index + 1],
                    (g_favorite_count - (size_t)index - 1) *
                    sizeof(g_favorite_ids[0]));
        g_favorite_count--;
        snprintf(g_scan_message, sizeof(g_scan_message),
                 "REMOVED FROM FAVORITES.");
    } else if (g_favorite_count < 256) {
        snprintf(g_favorite_ids[g_favorite_count],
                 sizeof(g_favorite_ids[g_favorite_count]), "%s", key);
        g_favorite_count++;
        snprintf(g_scan_message, sizeof(g_scan_message),
                 "ADDED TO FAVORITES.");
    } else {
        snprintf(g_scan_message, sizeof(g_scan_message),
                 "FAVORITES LIST IS FULL.");
        return;
    }
    if (save_favorites() != 0)
        snprintf(g_scan_message, sizeof(g_scan_message),
                 "FAVORITE SAVE FAILED.");
}

static int game_matches_filter(const ps3scale_game_t *game, int filter)
{
    if (!game) return 0;
    if (filter == LIBRARY_ALL || filter == LIBRARY_RECENT) return 1;
    if (filter == LIBRARY_PS3) return game_is_ps3(game);
    if (filter == LIBRARY_PS2) return game_is_ps2(game);
    if (filter == LIBRARY_FAVORITES) return game_is_favorite(game);
    return 0;
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
    return game_matches_filter(game, g_library) &&
           text_contains_ci(game->title, g_search);
}

static size_t library_count(int library)
{
    size_t i, count = 0;
    if (library == LIBRARY_RECENT) {
        for (i = 0; i < g_recent_count; i++) {
            const ps3scale_game_t *game =
                &g_games.items[g_recent_indices[i]];
            if (text_contains_ci(game->title, g_search)) count++;
        }
        return count;
    }
    for (i = 0; i < g_games.count; i++) {
        if (game_matches_filter(&g_games.items[i], library) &&
            text_contains_ci(g_games.items[i].title, g_search))
            count++;
    }
    return count;
}

static size_t library_total_count(int library)
{
    size_t i, count = 0;
    if (library == LIBRARY_ALL) return g_games.count;
    if (library == LIBRARY_RECENT) return g_recent_count;
    for (i = 0; i < g_games.count; i++)
        if (game_matches_filter(&g_games.items[i], library)) count++;
    return count;
}

static const ps3scale_game_t *library_game_at(size_t visible_index)
{
    size_t i, current = 0;
    if (g_library == LIBRARY_RECENT) {
        for (i = 0; i < g_recent_count; i++) {
            const ps3scale_game_t *game =
                &g_games.items[g_recent_indices[i]];
            if (!text_contains_ci(game->title, g_search)) continue;
            if (current++ == visible_index) return game;
        }
        return NULL;
    }
    for (i = 0; i < g_games.count; i++) {
        if (!game_matches_library(&g_games.items[i])) continue;
        if (current++ == visible_index) return &g_games.items[i];
    }
    return NULL;
}

static void rebuild_recent_indices(void)
{
    size_t i, j;
    g_recent_count = g_games.count;
    for (i = 0; i < g_recent_count; i++) {
        struct stat st;
        g_recent_indices[i] = (int)i;
        g_recent_mtimes[i] = 0;
        if (stat(g_games.items[i].path, &st) == 0)
            g_recent_mtimes[i] = st.st_mtime;
    }
    for (i = 1; i < g_recent_count; i++) {
        int index = g_recent_indices[i];
        time_t modified = g_recent_mtimes[i];
        j = i;
        while (j > 0 && g_recent_mtimes[j - 1] < modified) {
            g_recent_indices[j] = g_recent_indices[j - 1];
            g_recent_mtimes[j] = g_recent_mtimes[j - 1];
            j--;
        }
        g_recent_indices[j] = index;
        g_recent_mtimes[j] = modified;
    }
}



static const ps3scale_game_t *selected_game(void)
{
    size_t count = library_count(g_library);
    if (count == 0) return NULL;
    if (g_selected < 0 || (size_t)g_selected >= count) return NULL;
    return library_game_at((size_t)g_selected);
}

static void refresh_ps2_patch_availability(void)
{
    const ps3scale_game_t *game = selected_game();
    unsigned char *elf_data = NULL;
    size_t elf_size = 0;
    uint32_t elf_hash = 0;
    char serial[16];

    g_ps2_patch_probe_valid = 0;
    g_ps2_patch_fps_available = 0;
    g_ps2_patch_widescreen_available = 0;
    g_ps2_patch_480p_available = 0;
    g_ps2_patch_serial[0] = '\0';
    g_ps2_patch_elf_hash = 0;
    g_ps2_patch_confirm_pending = 0;
    if (!game || game->kind != PS3SCALE_GAME_PS2_ISO)
        return;

    if (!ps2_netemu_iso_supported()) {
        g_ps2_patch_probe_valid = -2;
        return;
    }

    g_ps2_patch_probe_valid = -1;
    if (ps3scale_ps2_iso_load_boot_elf(game->path, &elf_data, &elf_size,
            serial, sizeof(serial), &elf_hash) == 0) {
        g_ps2_patch_probe_valid = 1;
        snprintf(g_ps2_patch_serial, sizeof(g_ps2_patch_serial), "%s", serial);
        g_ps2_patch_elf_hash = elf_hash;
        g_ps2_patch_fps_available =
            ps3scale_ps2_game_patch_elf_addresses_valid(
                ps3scale_ps2_game_patch_find(serial, elf_hash,
                    PS3SCALE_PS2_PATCH_FPS), elf_data, elf_size);
        g_ps2_patch_widescreen_available =
            ps3scale_ps2_game_patch_elf_addresses_valid(
                ps3scale_ps2_game_patch_find(serial, elf_hash,
                    PS3SCALE_PS2_PATCH_WIDESCREEN), elf_data, elf_size);
        g_ps2_patch_480p_available =
            ps3scale_ps2_game_patch_elf_addresses_valid(
                ps3scale_ps2_game_patch_find(serial, elf_hash,
                    PS3SCALE_PS2_PATCH_480P), elf_data, elf_size);
    }
    free(elf_data);
}

static void clamp_selection(void)
{
    size_t count = library_count(g_library);
    if (count == 0) g_selected = 0;
    else if (g_selected >= (int)count) g_selected = (int)count - 1;
}

static int image_file_is_safe(const char *path, unsigned long max_bytes)
{
    struct stat st;

    if (!path || !path[0] || stat(path, &st) != 0 ||
        !S_ISREG(st.st_mode) || st.st_size <= 0 ||
        (unsigned long)st.st_size > max_bytes)
        return 0;
    return 1;
}

static void clear_scan_cover_cache(void)
{
    size_t i;
    for (i = 0; i < PS3SCALE_SCAN_GRID_COUNT; i++) {
        if (g_scan_cover_textures[i]) {
            ya2d_freeTexture(g_scan_cover_textures[i]);
            g_scan_cover_textures[i] = NULL;
        }
        g_scan_cover_paths[i][0] = '\0';
    }
    g_scan_cover_page = -1;
    g_scan_cover_library = -1;
}

static void update_scan_cover_cache(void)
{
    size_t i;
    size_t start = ((size_t)(g_selected < 0 ? 0 : g_selected) /
                    PS3SCALE_SCAN_GRID_COUNT) * PS3SCALE_SCAN_GRID_COUNT;
    if (g_scan_cover_page != (int)start ||
        g_scan_cover_library != g_library) {
        clear_scan_cover_cache();
        g_scan_cover_page = (int)start;
        g_scan_cover_library = g_library;
    }
    for (i = 0; i < PS3SCALE_SCAN_GRID_COUNT; i++) {
        const ps3scale_game_t *game = library_game_at(start + i);
        const char *cover = (game && game->cover_path[0]) ?
                            game->cover_path : "";
        if (strcmp(cover, g_scan_cover_paths[i]) == 0) continue;
        if (g_scan_cover_textures[i]) {
            ya2d_freeTexture(g_scan_cover_textures[i]);
            g_scan_cover_textures[i] = NULL;
        }
        snprintf(g_scan_cover_paths[i], sizeof(g_scan_cover_paths[i]),
                 "%s", cover);
        if (cover[0] && image_file_is_safe(cover, PS3SCALE_COVER_MAX_BYTES))
            g_scan_cover_textures[i] = ya2d_loadPNGfromFile(cover);
    }
}

static void update_selected_cover(void)
{
    const ps3scale_game_t *game;
    const char *path;
    if (g_screen == SCREEN_SCAN) {
        update_scan_cover_cache();
        return;
    }
    game = selected_game();
    path = (game && game->cover_path[0]) ? game->cover_path : "";
    if (strcmp(path, g_cover_path) == 0) return;
    if (g_cover_texture) {
        ya2d_freeTexture(g_cover_texture);
        g_cover_texture = NULL;
    }
    g_cover_path[0] = '\0';
    if (path[0] && image_file_is_safe(path, PS3SCALE_COVER_MAX_BYTES)) {
        snprintf(g_cover_path, sizeof(g_cover_path), "%s", path);
        g_cover_texture = ya2d_loadPNGfromFile(path);
    }
}

typedef struct ps3scale_scene_point {
    float x;
    float y;
    float z;
} ps3scale_scene_point_t;

typedef struct ps3scale_scene_face {
    float depth;
    float x[4];
    float y[4];
    u32 color;
    u32 vertex_color[4];
    u32 edge;
} ps3scale_scene_face_t;

#define PS3SCALE_STAIR_STEPS 32
#define PS3SCALE_STAIR_LOOP_FRAMES 21600ul
#define PS3SCALE_STAIR_FACE_MAX (PS3SCALE_STAIR_STEPS * 5)

static ps3scale_scene_point_t g_scene_camera_position;
static ps3scale_scene_point_t g_scene_camera_right;
static ps3scale_scene_point_t g_scene_camera_up;
static ps3scale_scene_point_t g_scene_camera_forward;

static ps3scale_scene_point_t scene_world_point(float center_x,
                                                 float center_y,
                                                 float center_z,
                                                 float radius,
                                                 float angle,
                                                 float y)
{
    ps3scale_scene_point_t point;
    point.x = center_x + cosf(angle) * radius;
    point.y = center_y + y;
    point.z = center_z + sinf(angle) * radius;
    return point;
}

/* Aim the software camera at the staircase while it follows a gentle,
   continuous dolly path. The camera basis is orthonormal and frame-local. */
static void scene_set_camera(float camera_x,
                             float camera_y,
                             float camera_z,
                             float target_x,
                             float target_y,
                             float target_z)
{
    float dx = target_x - camera_x;
    float dy = target_y - camera_y;
    float dz = target_z - camera_z;
    float horizontal = sqrtf(dx * dx + dz * dz);
    float yaw = atan2f(dx, dz);
    float pitch = atan2f(dy, horizontal);
    float cos_pitch = cosf(pitch);
    float sin_pitch = sinf(pitch);
    float sin_yaw = sinf(yaw);
    float cos_yaw = cosf(yaw);

    g_scene_camera_position.x = camera_x;
    g_scene_camera_position.y = camera_y;
    g_scene_camera_position.z = camera_z;

    g_scene_camera_forward.x = sin_yaw * cos_pitch;
    g_scene_camera_forward.y = sin_pitch;
    g_scene_camera_forward.z = cos_yaw * cos_pitch;

    g_scene_camera_right.x = cos_yaw;
    g_scene_camera_right.y = 0.0f;
    g_scene_camera_right.z = -sin_yaw;

    g_scene_camera_up.x = -sin_yaw * sin_pitch;
    g_scene_camera_up.y = cos_pitch;
    g_scene_camera_up.z = -cos_yaw * sin_pitch;
}

static ps3scale_scene_point_t scene_camera_space_point(
    ps3scale_scene_point_t point)
{
    ps3scale_scene_point_t relative, camera;
    relative.x = point.x - g_scene_camera_position.x;
    relative.y = point.y - g_scene_camera_position.y;
    relative.z = point.z - g_scene_camera_position.z;

    camera.x = relative.x * g_scene_camera_right.x +
               relative.y * g_scene_camera_right.y +
               relative.z * g_scene_camera_right.z;
    camera.y = relative.x * g_scene_camera_up.x +
               relative.y * g_scene_camera_up.y +
               relative.z * g_scene_camera_up.z;
    camera.z = relative.x * g_scene_camera_forward.x +
               relative.y * g_scene_camera_forward.y +
               relative.z * g_scene_camera_forward.z;
    return camera;
}

static void scene_project_camera_point(ps3scale_scene_point_t point,
                                       float focal_length,
                                       float *screen_x,
                                       float *screen_y)
{
    float depth = point.z;
    if (depth < 2.5f) depth = 2.5f;
    *screen_x = 427.0f + point.x * focal_length / depth;
    *screen_y = 256.0f - point.y * focal_length / depth;
}

static u32 scene_blend(u32 a, u32 b, float t);

static void scene_push_face(ps3scale_scene_face_t *faces,
                            size_t *face_count,
                            const ps3scale_scene_point_t points[4],
                            float focal_length,
                            u32 color,
                            u32 edge)
{
    ps3scale_scene_face_t *face;
    ps3scale_scene_point_t camera_points[4];
    float depth = 0.0f;
    int i;
    if (!faces || !face_count || *face_count >= PS3SCALE_STAIR_FACE_MAX)
        return;

    /* Drop faces crossing the near plane instead of stretching them. */
    for (i = 0; i < 4; i++) {
        camera_points[i] = scene_camera_space_point(points[i]);
        if (camera_points[i].z <= 2.5f)
            return;
    }

    face = &faces[(*face_count)++];
    for (i = 0; i < 4; i++) {
        depth += camera_points[i].z * 0.25f;
        scene_project_camera_point(camera_points[i], focal_length,
                                   &face->x[i], &face->y[i]);
    }
    face->depth = depth;
    face->color = color;
    face->edge = edge;
    /* Pearl-like directional sheen at the front/outside corners plus
       subtle depth haze. This is per-vertex shading (no expensive textures,
       screen-space passes or per-frame allocations). */
    for (i = 0; i < 4; i++) {
        float haze = (camera_points[i].z - 25.0f) / 58.0f;
        float reflection = (i == 1 || i == 2) ? 0.15f : 0.045f;
        u32 shaded = scene_blend(color, 0xf7f4fcff, haze * 0.34f);
        face->vertex_color[i] =
            scene_blend(shaded, 0xffffffff, reflection);
    }
}

static int compare_scene_face_depth(const void *left, const void *right)
{
    const ps3scale_scene_face_t *a = (const ps3scale_scene_face_t *)left;
    const ps3scale_scene_face_t *b = (const ps3scale_scene_face_t *)right;
    if (a->depth < b->depth) return 1;
    if (a->depth > b->depth) return -1;
    return 0;
}

static void draw_scene_faces(const ps3scale_scene_face_t *faces,
                             size_t face_count)
{
    size_t i;
    int j;
    if (!faces || face_count == 0)
        return;

    /* Keep painter order but submit all tread surfaces in one quad batch. */
    tiny3d_SetPolygon(TINY3D_QUADS);
    for (i = 0; i < face_count; i++) {
        for (j = 0; j < 4; j++) {
            tiny3d_VertexPos(faces[i].x[j], faces[i].y[j], 65535.0f);
            tiny3d_VertexColor(faces[i].vertex_color[j]);
        }
    }
    tiny3d_End();

    /* Batch every tread outline as line pairs instead of one draw per face. */
    tiny3d_SetPolygon(TINY3D_LINES);
    for (i = 0; i < face_count; i++) {
        if (faces[i].edge == 0)
            continue; /* No wireframe on shaded risers or undersides. */
        for (j = 0; j < 4; j++) {
            int next = (j + 1) & 3;
            tiny3d_VertexPos(faces[i].x[j], faces[i].y[j], 65535.0f);
            tiny3d_VertexColor(faces[i].edge);
            tiny3d_VertexPos(faces[i].x[next], faces[i].y[next], 65535.0f);
        }
    }
    tiny3d_End();
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

static u32 ui_blend_color(u32 a, u32 b, float t)
{
    int alpha_a = (int)(a & 255u);
    int alpha_b = (int)(b & 255u);
    int alpha;

    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    alpha = alpha_a + (int)((float)(alpha_b - alpha_a) * t);

    /* scene_blend intentionally forces opaque scene colors; UI cards must
       interpolate their alpha too so translucent shadows stay translucent. */
    return (scene_blend(a, b, t) & 0xffffff00u) | (u32)alpha;
}

static void draw_scene_background(void)
{
    const float tau = 6.28318530718f;
    unsigned long frame_in_loop =
        g_frame_counter % PS3SCALE_STAIR_LOOP_FRAMES;
    float phase = (float)frame_in_loop /
                  (float)PS3SCALE_STAIR_LOOP_FRAMES * tau;
    float daylight = 0.82f + 0.12f * cosf(phase);
    float night = 1.0f - daylight;
    float dusk = 1.0f - fabsf(daylight - 0.82f) / 0.12f;
    u32 top, bottom, color;
    int i, wave;

    if (!g_animation_enabled) {
        phase = 0.0f;
        daylight = 0.86f;
        night = 0.14f;
        dusk = 0.0f;
    }

    top = scene_blend(0xe8e5f0ff, 0xfdfcffff, daylight);
    bottom = scene_blend(0xd8d2e7ff, 0xf2eff9ff, daylight);
    top = scene_blend(top, 0xe9defcff, dusk * 0.06f);
    bottom = scene_blend(bottom, 0xe0d4f5ff, dusk * 0.08f);

    /* Batch the 24 gradient strips into one RSX submission. */
    tiny3d_SetPolygon(TINY3D_QUADS);
    for (i = 0; i < 24; i++) {
        float t = (float)i / 23.0f;
        float y = (float)i * 22.0f;
        color = ui_theme_color(scene_blend(top, bottom, t));
        tiny3d_VertexPos(0.0f, y, 65535.0f);
        tiny3d_VertexColor(color);
        tiny3d_VertexPos(854.0f, y, 65535.0f);
        tiny3d_VertexColor(color);
        tiny3d_VertexPos(854.0f, y + 23.0f, 65535.0f);
        tiny3d_VertexColor(color);
        tiny3d_VertexPos(0.0f, y + 23.0f, 65535.0f);
        tiny3d_VertexColor(color);
    }
    tiny3d_End();

    /* Light ribbons and particles retain the background depth without rings. */
    /* All three light ribbons are submitted in one line batch. */
    tiny3d_SetPolygon(TINY3D_LINES);
    for (wave = 0; wave < 3; wave++) {
        float previous_x = 0.0f;
        float previous_y = 0.0f;
        int segment;
        for (segment = 0; segment <= 28; segment++) {
            float x = (float)segment * (854.0f / 28.0f);
            float y = 430.0f - (float)wave * 42.0f +
                sinf(phase + x * 0.0065f + (float)wave * 0.85f) *
                (8.0f + (float)wave * 2.0f);
            if (segment > 0) {
                color = ui_theme_color(
                    scene_blend(0xc4b1f5ff, 0xffffffcc,
                                daylight * 0.72f));
                tiny3d_VertexPos(previous_x, previous_y, 65535.0f);
                tiny3d_VertexColor(color);
                tiny3d_VertexPos(x, y, 65535.0f);
                tiny3d_VertexColor(color);
            }
            previous_x = x;
            previous_y = y;
        }
    }
    tiny3d_End();

    /* Batch both particle layers into one quad submission. */
    tiny3d_SetPolygon(TINY3D_QUADS);
    for (i = 0; i < 22; i++) {
        float x = (float)((i * 137 + 31) % 842);
        float y = (float)((i * 83 + 19) % 470) +
                  sinf(phase + (float)i * 0.73f) * 3.0f;
        float pulse = 0.45f + 0.55f *
                      sinf(phase * 2.0f + (float)i * 1.17f);
        float size = (i % 5 == 0) ? 3.0f : 2.0f;
        color = ui_theme_color(
            scene_blend(0x9e8bcf00, 0xffffffff,
                        pulse * daylight * 0.65f));
        tiny3d_VertexPos(x, y, 65535.0f);
        tiny3d_VertexColor(color);
        tiny3d_VertexPos(x + size, y, 65535.0f);
        tiny3d_VertexColor(color);
        tiny3d_VertexPos(x + size, y + size, 65535.0f);
        tiny3d_VertexColor(color);
        tiny3d_VertexPos(x, y + size, 65535.0f);
        tiny3d_VertexColor(color);
    }

    for (i = 0; i < 14; i++) {
        float x = (float)((i * 173 + 67) % 840);
        float y = (float)((i * 97 + 33) % 300);
        color = ui_theme_color(
            scene_blend(0x66558a00, 0xf6edffcc, night * 0.82f));
        tiny3d_VertexPos(x, y, 65535.0f);
        tiny3d_VertexColor(color);
        tiny3d_VertexPos(x + 2.0f, y, 65535.0f);
        tiny3d_VertexColor(color);
        tiny3d_VertexPos(x + 2.0f, y + 2.0f, 65535.0f);
        tiny3d_VertexColor(color);
        tiny3d_VertexPos(x, y + 2.0f, 65535.0f);
        tiny3d_VertexColor(color);
    }
    tiny3d_End();
    (void)dusk;
}

/* Add one camera-visible segment to an already-open Tiny3D line batch. */
static void scene_emit_line_segment(ps3scale_scene_point_t a,
                                    ps3scale_scene_point_t b,
                                    float focal_length,
                                    u32 color)
{
    ps3scale_scene_point_t camera_a = scene_camera_space_point(a);
    ps3scale_scene_point_t camera_b = scene_camera_space_point(b);
    float x1, y1, x2, y2;
    if (camera_a.z <= 2.5f || camera_b.z <= 2.5f)
        return;
    scene_project_camera_point(camera_a, focal_length, &x1, &y1);
    scene_project_camera_point(camera_b, focal_length, &x2, &y2);
    tiny3d_VertexPos(x1, y1, 65535.0f);
    tiny3d_VertexColor(color);
    tiny3d_VertexPos(x2, y2, 65535.0f);
}

static void draw_home_ambient(void)
{
    ps3scale_scene_face_t faces[PS3SCALE_STAIR_FACE_MAX];
    const float tau = 6.28318530718f;
    const float center_x = 7.2f;
    float center_y;
    const float center_z = 34.0f;
    const float inner_radius = 2.0f;
    const float outer_radius = 14.0f;
    const float angle_step = tau / 18.0f;
    const float rise_per_tread = 0.72f;
    const float focal_length = 780.0f;
    unsigned long frame_in_loop =
        g_frame_counter % PS3SCALE_STAIR_LOOP_FRAMES;
    float phase = (float)frame_in_loop /
                  (float)PS3SCALE_STAIR_LOOP_FRAMES * tau;
    /* Advance continuously through one full spiral revolution. The camera
       and the local geometry origin share the same vertical offset, so the
       view returns to its exact starting pose at the loop boundary. */
    float camera_steps = phase / tau * 18.0f;
    float camera_y = camera_steps * rise_per_tread;
    size_t face_count = 0, i;
    int j;

    center_y = camera_y;
    scene_set_camera(0.0f, camera_y, 0.0f,
                     center_x, camera_y, center_z);

    /* One spiral staircase only; the scene background supplies no second flight. */

    /* White/silver treads stream past the camera with no visible end. */
    for (i = 0; i < PS3SCALE_STAIR_STEPS; i++) {
        float angle = ((float)i + camera_steps) * angle_step;
        float next_angle = angle + angle_step * 0.92f;
        float y = ((float)i - (float)(PS3SCALE_STAIR_STEPS - 1) * 0.5f) *
                  rise_per_tread;
        ps3scale_scene_point_t top[4], riser[4], side[4];
        ps3scale_scene_point_t pearl_lip[4], contact_shadow[4];
        float light = 0.68f + 0.25f * cosf(angle + 0.42f);
        u32 top_color = scene_blend(0xe3ddecff, 0xfffeffff, light);
        u32 riser_color = scene_blend(0x988eafff, 0xc7bedaff, light);
        u32 side_color = scene_blend(0xa99fbeff, 0xd5cde5ff, light);
        u32 lip_color = scene_blend(0xded7efff, 0xfffeffff, light);
        u32 contact_color = scene_blend(0x6f6788ff, 0xa39abfff, light);

        top[0] = scene_world_point(center_x, center_y, center_z,
                                   inner_radius, angle, y + 0.14f);
        top[1] = scene_world_point(center_x, center_y, center_z,
                                   outer_radius, angle, y + 0.14f);
        top[2] = scene_world_point(center_x, center_y, center_z,
                                   outer_radius, next_angle, y + 0.14f);
        top[3] = scene_world_point(center_x, center_y, center_z,
                                   inner_radius, next_angle, y + 0.14f);

        riser[0] = scene_world_point(center_x, center_y, center_z,
                                     outer_radius, angle, y - 0.20f);
        riser[1] = scene_world_point(center_x, center_y, center_z,
                                     outer_radius, next_angle, y - 0.20f);
        /* End the riser at the underside of the bevel: coplanar faces
           do not overlap, so the pearl highlight cannot flicker/vanish. */
        riser[2] = scene_world_point(center_x, center_y, center_z,
                                     outer_radius, next_angle, y + 0.025f);
        riser[3] = scene_world_point(center_x, center_y, center_z,
                                     outer_radius, angle, y + 0.025f);

        side[0] = scene_world_point(center_x, center_y, center_z,
                                    inner_radius, next_angle, y - 0.20f);
        side[1] = riser[1];
        side[2] = top[2];
        side[3] = top[3];

        /* Polished bevel on the visible outer tread edge. The thin darker
           strip under it gives the steps real thickness and contact shadow
           instead of a flat 2D paper staircase. */
        pearl_lip[0] = top[1];
        pearl_lip[1] = top[2];
        pearl_lip[2] = scene_world_point(center_x, center_y, center_z,
                                         outer_radius, next_angle, y + 0.025f);
        pearl_lip[3] = scene_world_point(center_x, center_y, center_z,
                                         outer_radius, angle, y + 0.025f);

        contact_shadow[0] = riser[0];
        contact_shadow[1] = riser[1];
        contact_shadow[2] = scene_world_point(center_x, center_y, center_z,
                                              outer_radius, next_angle, y - 0.29f);
        contact_shadow[3] = scene_world_point(center_x, center_y, center_z,
                                              outer_radius, angle, y - 0.29f);

        scene_push_face(faces, &face_count, top, focal_length,
                        top_color, 0); /* Bevel supplies the visible edge. */
        scene_push_face(faces, &face_count, riser, focal_length,
                        riser_color, 0);
        scene_push_face(faces, &face_count, side, focal_length,
                        side_color, 0);
        scene_push_face(faces, &face_count, pearl_lip, focal_length,
                        lip_color, 0xfaf6ffff);
        scene_push_face(faces, &face_count, contact_shadow, focal_length,
                        contact_color, 0);
    }

    /* Painter's order gives reliable depth without relying on RSX Z state. */
    qsort(faces, face_count, sizeof(faces[0]), compare_scene_face_depth);
    draw_scene_faces(faces, face_count);

    /* Inner and outer spiral handrails are batched to keep frame cost low. */
    tiny3d_SetPolygon(TINY3D_LINES);
    for (j = 0; j < PS3SCALE_STAIR_STEPS * 6; j++) {
        float t = (float)j / 6.0f;
        float next_t = (float)(j + 1) / 6.0f;
        float angle = (t + camera_steps) * angle_step;
        float next_angle = (next_t + camera_steps) * angle_step;
        float y = (t - (float)(PS3SCALE_STAIR_STEPS - 1) * 0.5f) *
                  0.72f;
        float next_y = (next_t - (float)(PS3SCALE_STAIR_STEPS - 1) * 0.5f) *
                       0.72f;
        ps3scale_scene_point_t outer_a, outer_b, inner_a, inner_b;
        outer_a = scene_world_point(center_x, center_y, center_z,
                                    outer_radius + 0.62f, angle, y + 1.15f);
        outer_b = scene_world_point(center_x, center_y, center_z,
                                    outer_radius + 0.62f, next_angle,
                                    next_y + 1.15f);
        inner_a = scene_world_point(center_x, center_y, center_z,
                                    inner_radius + 0.42f, angle, y + 0.92f);
        inner_b = scene_world_point(center_x, center_y, center_z,
                                    inner_radius + 0.42f, next_angle,
                                    next_y + 0.92f);
        scene_emit_line_segment(outer_a, outer_b, focal_length,
                                0xa18ad2ff);
        scene_emit_line_segment(inner_a, inner_b, focal_length,
                                0xcdbfe8cc);
    }

    /* Fine balusters and tread-edge glints complete the staircase silhouette. */
    for (i = 0; i < PS3SCALE_STAIR_STEPS; i += 3) {
        float angle = ((float)i + camera_steps) * angle_step;
        float y = ((float)i - (float)(PS3SCALE_STAIR_STEPS - 1) * 0.5f) *
                  0.72f;
        ps3scale_scene_point_t base, rail, edge_a, edge_b;
        base = scene_world_point(center_x, center_y, center_z,
                                 outer_radius, angle, y + 0.16f);
        rail = scene_world_point(center_x, center_y, center_z,
                                 outer_radius + 0.62f, angle, y + 1.15f);
        edge_a = scene_world_point(center_x, center_y, center_z,
                                   outer_radius, angle, y + 0.14f);
        edge_b = scene_world_point(center_x, center_y, center_z,
                                   outer_radius, angle + angle_step * 0.92f,
                                   y + 0.14f);
        scene_emit_line_segment(base, rail, focal_length, 0xa894d3bb);
        scene_emit_line_segment(edge_a, edge_b, focal_length, 0xe6def4e8);
    }
    tiny3d_End();
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
    int profile_downloaded = 0;
    int community_index_downloaded = 0;
    (void)arg;
    g_catalog_sync_result = 0;
    g_catalog_sync_stage = 1;
    g_catalog_sync_downloaded = 0;
    g_catalog_sync_total = 0;
    ps3scale_community_index_free(&g_downloaded_community_index);

    if (ps3scale_online_download_progress(
            PS3SCALE_CATALOG_URL, PS3SCALE_CATALOG_DOWNLOAD_PATH,
            &g_catalog_sync_downloaded, &g_catalog_sync_total) == 0) {
        profile_downloaded = 1;
        if (ps3scale_catalog_load(PS3SCALE_CATALOG_DOWNLOAD_PATH,
                                  &g_downloaded_catalog) > 0 &&
            rename(PS3SCALE_CATALOG_DOWNLOAD_PATH,
                   PS3SCALE_CATALOG_CACHE_PATH) == 0) {
            result |= 1;
            profile_downloaded = 0;
        }
    }
    if (profile_downloaded)
        (void)unlink(PS3SCALE_CATALOG_DOWNLOAD_PATH);

    g_catalog_sync_stage = 2;
    g_catalog_sync_downloaded = 0;
    g_catalog_sync_total = 0;
    if (ps3scale_online_download_limited_progress(
            PS3SCALE_COMMUNITY_INDEX_URL,
            PS3SCALE_COMMUNITY_INDEX_DOWNLOAD_PATH,
            PS3SCALE_COMMUNITY_INDEX_DOWNLOAD_LIMIT,
            &g_catalog_sync_downloaded, &g_catalog_sync_total) == 0) {
        community_index_downloaded = 1;
        if (ps3scale_community_index_load(
                PS3SCALE_COMMUNITY_INDEX_DOWNLOAD_PATH,
                &g_downloaded_community_index) > 0 &&
            rename(PS3SCALE_COMMUNITY_INDEX_DOWNLOAD_PATH,
                   PS3SCALE_COMMUNITY_INDEX_CACHE_PATH) == 0) {
            result |= 2;
            community_index_downloaded = 0;
        }
    }
    if (community_index_downloaded) {
        (void)unlink(PS3SCALE_COMMUNITY_INDEX_DOWNLOAD_PATH);
        ps3scale_community_index_free(&g_downloaded_community_index);
    }

    g_catalog_sync_result = result;
    g_catalog_sync_stage = 0;

    /* Publish completion before clearing running so UI actions never
     * overlap the final network teardown window. */
    g_catalog_sync_done = 1;
    g_catalog_sync_running = 0;
    g_network_task_running = 0;
}

static void start_catalog_sync_worker(int force)
{
    int rc;

    if (g_network_task_running ||
        g_catalog_sync_running || g_catalog_sync_done ||
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
    g_catalog_sync_stage = 1;
    g_catalog_sync_downloaded = 0;
    g_catalog_sync_total = 0;
    g_catalog_sync_running = 1;
    g_network_task_running = 1;
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
        g_catalog_sync_stage = 0;
        g_catalog_sync_downloaded = 0;
        g_catalog_sync_total = 0;
        g_network_task_running = 0;
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
    g_catalog_sync_stage = 0;
    g_catalog_sync_downloaded = 0;
    g_catalog_sync_total = 0;
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
        /* Installed-game matches are preferred; unmatched index entries remain browse-only. */
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

            if (!matched_installed_game && !exact_only &&
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

    /* PS2 configs are compatibility files, not graphics upgrades.
       Show them only under ALL FILES, and only for an explicit search. */
    if (!graphics_only && g_online_query[0] && g_ps2_config_db.items) {
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
    ps3scale_game_t mounted;
    int game_valid = 0;
    (void)arg;

    /* Re-read the exact Title ID + APP_VER away from the controller thread.
       A full HDD scan is deliberately never performed on the UI stack. */
    if (g_profile_arm_game_target.title_id[0] &&
        g_profile_arm_game_target.app_ver[0]) {
        if (g_profile_arm_game_target.kind == PS3SCALE_GAME_PS3_HDD)
            game_valid = find_installed_game(&g_profile_arm_game_target);
        else
            game_valid = find_mounted_game(&g_profile_arm_game_target, &mounted);
    }

    if (!game_valid) {
        g_profile_arm_result = -102;
    } else if (ps3scale_webman_available() != 0) {
        g_profile_arm_result = -100;
    } else if (ps3scale_webman_ps3mapi_available() != 0) {
        g_profile_arm_result = -101;
    } else {
        g_profile_arm_result =
            ps3scale_profile_script_install(&g_profile_arm_target);
    }

    g_profile_arm_done = 1;
    g_profile_arm_running = 0;
    g_network_task_running = 0;
}

static int selected_game_matches_target(const ps3scale_game_t *target)
{
    const ps3scale_game_t *current = selected_game();
    return target && g_screen == SCREEN_DETAILS && current &&
           strcmp(current->title_id, target->title_id) == 0 &&
           strcmp(current->app_ver, target->app_ver) == 0 &&
           strcmp(current->path, target->path) == 0;
}

static int selected_game_matches_auto_launch_target(void)
{
    return selected_game_matches_target(&g_auto_launch_target);
}

static int selected_game_matches_game_launch_target(void)
{
    return selected_game_matches_target(&g_game_launch_target);
}

static void service_profile_arm(void)
{
    int result;
    int auto_launch;
    int cleanup_result;

    if (!g_profile_arm_done)
        return;

    if (g_profile_arm_thread_started) {
        u64 thread_result = 0;
        (void)sysThreadJoin(g_profile_arm_thread, &thread_result);
        g_profile_arm_thread_started = 0;
    }

    result = g_profile_arm_result;
    auto_launch = g_auto_launch_after_profile_arm;
    g_auto_launch_after_profile_arm = 0;
    g_profile_arm_done = 0;
    g_profile_arm_running = 0;
    g_details_cache_valid = 0;

    if (result == 0 || result == 1) {
        if (auto_launch) {
            if (selected_game_matches_auto_launch_target()) {
                set_details_message("PROFILE ARMED. STARTING GAME...");
                start_selected_game();
            } else {
                (void)ps3scale_profile_script_remove(
                    g_auto_launch_target.title_id);
                set_details_message(
                    "GAME SELECTION CHANGED. PROFILE REMOVED; NOT LAUNCHED.");
            }
        } else {
            set_details_message("ONE-LAUNCH PROFILE ARMED. START THIS GAME.");
        }
    } else if (auto_launch &&
               (result == -1 || result == -4 || result == -5 ||
                result == -101 || result == -102)) {
        /* A failed/unsafe enhancement must not prevent a normal launch when
           webMAN can still start the selected title. Keep selection identity. */
        if (selected_game_matches_auto_launch_target()) {
            /* Remove only an owned leftover script. A foreign/unreadable script
               blocks fallback rather than being launched or overwritten. */
            cleanup_result = ps3scale_profile_script_remove(
                g_auto_launch_target.title_id);
            if (cleanup_result == -2) {
                set_details_message(
                    "USER WEBMAN SCRIPT EXISTS OR CANNOT BE VERIFIED. LAUNCH CANCELLED.");
            } else {
                g_force_unmodified_launch = 1;
                set_details_message("PROFILE NOT APPLIED. STARTING GAME UNMODIFIED.");
                start_selected_game();
            }
        } else {
            set_details_message("GAME SELECTION CHANGED. NO GAME WAS STARTED.");
        }
    } else if (result == -100) {
        set_details_message("WEBMAN MOD NOT RESPONDING. PROFILE NOT ARMED.");
    } else if (result == -101) {
        set_details_message("PS3MAPI NOT ACTIVE. PROFILE NOT ARMED.");
    } else if (result == -102) {
        set_details_message("EXACT GAME VERSION NOT FOUND. RESCAN OR MOUNT THE GAME.");
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
    if (strcmp(profile->trust, "B_COMMUNITY") != 0) {
        set_details_message("PROFILE TRUST LEVEL DOES NOT ALLOW AUTO APPLY.");
        g_profile_confirm_pending = 0;
        return;
    }
    if (profile->patch_count != 1) {
        set_details_message("MULTI-WRITE PROFILE DISABLED FOR AUTO APPLY SAFETY.");
        g_profile_confirm_pending = 0;
        return;
    }
    if (g_network_task_running ||
        g_catalog_sync_running || g_catalog_sync_done ||
        g_game_launch_running || g_game_launch_done ||
        g_profile_arm_running || g_profile_arm_done) {
        set_details_message("WAIT FOR ONLINE DATABASE OR GAME ACTION TO FINISH.");
        g_profile_confirm_pending = 0;
        return;
    }

    if (!game || !game->title_id[0] || !game->app_ver[0]) {
        set_details_message("GAME TITLE ID OR APP VERSION IS MISSING. RESCAN.");
        g_profile_confirm_pending = 0;
        return;
    }
    /* Copy both identities before the worker starts; it owns the disk scan. */
    g_profile_arm_target = *profile;
    g_profile_arm_game_target = *game;
    g_profile_arm_result = -1;
    g_profile_arm_done = 0;
    g_profile_arm_running = 1;
    g_network_task_running = 1;
    g_profile_confirm_pending = 0;
    set_details_message("CHECKING GAME VERSION, WEBMAN AND ARMING PROFILE.");

    result = sysThreadCreate(&g_profile_arm_thread, profile_arm_worker, NULL,
                             1500, 0x8000, THREAD_JOINABLE,
                             "PS3 SCALE Profile Arm");
    if (result == 0) {
        g_profile_arm_thread_started = 1;
    } else {
        g_profile_arm_running = 0;
        g_profile_arm_done = 0;
        g_network_task_running = 0;
        set_details_message("COULD NOT START PROFILE WORKER. NO PATCH WAS ARMED.");
    }
}

static int find_mounted_game(const ps3scale_game_t *selected, ps3scale_game_t *mounted)
{
    size_t i;

    if (!selected || !mounted || !selected->title_id[0])
        return 0;

    ps3scale_game_list_init(&g_validation_scan);
    if (ps3scale_scan_device("/dev_bdvd", &g_validation_scan) <= 0)
        return 0;

    for (i = 0; i < g_validation_scan.count; i++) {
        const ps3scale_game_t *candidate = &g_validation_scan.items[i];
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
    size_t i;

    if (!selected || selected->kind != PS3SCALE_GAME_PS3_HDD ||
        !selected->title_id[0] || !selected->app_ver[0])
        return 0;

    ps3scale_game_list_init(&g_validation_scan);
    if (ps3scale_scan_device("/dev_hdd0", &g_validation_scan) <= 0)
        return 0;

    for (i = 0; i < g_validation_scan.count; i++) {
        const ps3scale_game_t *candidate = &g_validation_scan.items[i];
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
    ps3scale_game_t mounted;
    int identity_valid = 1;
    int cleanup_result;
    (void)arg;

    if (g_game_launch_profile_armed) {
        identity_valid = g_game_launch_target.title_id[0] &&
                         g_game_launch_target.app_ver[0];
        if (identity_valid &&
            g_game_launch_target.kind == PS3SCALE_GAME_PS3_HDD)
            identity_valid = find_installed_game(&g_game_launch_target);
        else if (identity_valid)
            identity_valid = find_mounted_game(&g_game_launch_target, &mounted);

        if (!identity_valid) {
            /* Never run a profile against a different or unmounted revision. */
            cleanup_result = ps3scale_profile_script_remove(
                g_game_launch_target.title_id);
            g_game_launch_result = cleanup_result == -2 ?
                PS3SCALE_GAME_LAUNCH_ERROR_SCRIPT :
                PS3SCALE_GAME_LAUNCH_ERROR_IDENTITY;
            goto launch_finished;
        }

        if (ps3scale_webman_ps3mapi_available() != 0) {
            /* A missing PS3MAPI endpoint must not block a normal launch.
               Continue only if our own one-launch script is absent/removed. */
            cleanup_result = ps3scale_profile_script_remove(
                g_game_launch_target.title_id);
            if (cleanup_result == -2) {
                g_game_launch_result = PS3SCALE_GAME_LAUNCH_ERROR_SCRIPT;
                goto launch_finished;
            }
            g_game_launch_profile_skipped = 1;
        }
    }
    g_game_launch_result =
        ps3scale_webman_start_game(&g_game_launch_target);

launch_finished:
    g_game_launch_done = 1;
    g_game_launch_running = 0;
    g_network_task_running = 0;
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
        set_details_message(g_game_launch_profile_skipped ?
            "PS3MAPI NOT ACTIVE. GAME STARTED WITHOUT PROFILE." :
            "WEBMAN ACCEPTED START REQUEST.");
        g_exit_requested = 1;
    } else if (g_game_launch_result == PS3SCALE_GAME_LAUNCH_ERROR_IDENTITY) {
        if (selected_game_matches_game_launch_target()) {
            g_force_unmodified_launch = 1;
            set_details_message("PROFILE REMOVED. STARTING GAME UNMODIFIED.");
            start_selected_game();
        } else {
            set_details_message("GAME SELECTION CHANGED. PROFILE REMOVED; NOT LAUNCHED.");
        }
    } else if (g_game_launch_result == PS3SCALE_GAME_LAUNCH_ERROR_SCRIPT) {
        set_details_message("PROFILE SCRIPT COULD NOT BE SAFELY REMOVED. GAME NOT STARTED.");
    } else if (g_game_launch_result == -2) {
        set_details_message("PS3MAPI NOT ACTIVE. PROFILE WAS NOT APPLIED.");
    } else if (g_game_launch_result == PS3SCALE_WEBMAN_ERROR_NETWORK_INIT) {
        set_details_message("PS3 NETWORK MODULES FAILED TO INITIALIZE.");
    } else if (g_game_launch_result == PS3SCALE_WEBMAN_ERROR_SOCKET) {
        set_details_message("PS3 NETWORK SOCKET COULD NOT BE OPENED.");
    } else if (g_game_launch_result == PS3SCALE_WEBMAN_ERROR_CONNECT) {
        set_details_message("WEBMAN MOD NOT RESPONDING ON LOCAL HTTP PORT 80.");
    } else if (g_game_launch_result == PS3SCALE_WEBMAN_ERROR_SEND) {
        set_details_message("WEBMAN LAUNCH REQUEST COULD NOT BE SENT.");
    } else if (g_game_launch_result == PS3SCALE_WEBMAN_ERROR_RESPONSE) {
        set_details_message("WEBMAN DID NOT RETURN AN HTTP RESPONSE.");
    } else if (g_game_launch_result == PS3SCALE_WEBMAN_ERROR_HTTP_STATUS) {
        set_details_message("WEBMAN REJECTED THE GAME TITLE OR PATH.");
    } else if (g_game_launch_result == PS3SCALE_WEBMAN_ERROR_INVALID_REQUEST) {
        set_details_message("GAME TITLE ID OR PATH IS INVALID. RESCAN.");
    } else {
        snprintf(g_details_message, sizeof(g_details_message),
                 "WEBMAN START FAILED (ERROR %d).",
                 (int)g_game_launch_result);
    }
}

static void start_selected_game(void)
{
    const ps3scale_game_t *game = selected_game();
    const ps3scale_profile_t *profile = NULL;
    int script_state = 0;
    int rc;
    int cleanup_result;
    int force_unmodified = g_force_unmodified_launch;
    int is_ps2_iso = game && game->kind == PS3SCALE_GAME_PS2_ISO;

    g_force_unmodified_launch = 0;

    if (!game || (!game_is_ps3(game) && !is_ps2_iso)) {
        set_details_message("PS2 CLASSICS MUST BE STARTED WITH THEIR COMPATIBLE LAUNCHER.");
        return;
    }
    if (!is_ps2_iso && !game->title_id[0]) {
        set_details_message("TITLE ID NOT AVAILABLE. RESCAN THE GAME.");
        return;
    }
    if (force_unmodified && !is_ps2_iso) {
        /* Final guard against a stale owned script or a newly-created user script. */
        cleanup_result = ps3scale_profile_script_remove(game->title_id);
        if (cleanup_result == -2) {
            set_details_message(
                "USER WEBMAN SCRIPT EXISTS OR CANNOT BE VERIFIED. LAUNCH CANCELLED.");
            return;
        }
    }
    if (g_network_task_running ||
        g_game_launch_running || g_game_launch_done ||
        g_profile_arm_running || g_profile_arm_done ||
        g_ps2_config_download_running || g_ps2_config_download_done) {
        set_details_message("WAIT FOR THE CURRENT GAME OR CONFIG TASK TO FINISH.");
        return;
    }
    if (g_catalog_sync_running || g_catalog_sync_done) {
        set_details_message("WAIT FOR ONLINE DATABASE SYNC TO FINISH BEFORE STARTING.");
        return;
    }

    if (!is_ps2_iso && !force_unmodified) {
        profile = profile_for_game(game);
        script_state = profile_script_state_for_game(game, profile);
        if (script_state == -2) {
            set_details_message("STALE PROFILE SCRIPT. REMOVE IT BEFORE STARTING.");
            return;
        }
        if (profile && script_state == -1) {
            set_details_message("USER WEBMAN SCRIPT EXISTS. PROFILE NOT OVERWRITTEN.");
            return;
        }
        if (profile && strcmp(profile->trust, "B_COMMUNITY") != 0) {
            set_details_message("PROFILE IS NOT ELIGIBLE FOR AUTOMATIC APPLY.");
            return;
        }
        if (profile && profile->patch_count != 1) {
            set_details_message("MULTI-WRITE PROFILE DISABLED FOR AUTO APPLY SAFETY.");
            return;
        }

        /* CROSS/START is explicit consent to arm an exact one-write profile
           and launch only after the one-launch script has been installed. */
        if (profile && script_state == 0) {
            g_auto_launch_target = *game;
            g_auto_launch_after_profile_arm = 1;
            confirm_profile_install();
            if (!g_profile_arm_running && !g_profile_arm_done)
                g_auto_launch_after_profile_arm = 0;
            return;
        }

        /* The launch worker revalidates the exact identity off the UI thread. */
    }

    g_game_launch_target = *game;
    g_game_launch_profile_armed = !is_ps2_iso && (script_state == 1);
    g_game_launch_profile_skipped = 0;
    g_game_launch_result = -1;
    g_game_launch_done = 0;
    g_game_launch_running = 1;
    g_network_task_running = 1;

    if (is_ps2_iso)
        set_details_message("MOUNTING PS2 ISO VIA WEBMAN. WAIT FOR THE RESPONSE.");
    else if (script_state == 1)
        set_details_message("STARTING GAME WITH ONE-LAUNCH PROFILE.");
    else if (force_unmodified)
        set_details_message("PROFILE NOT APPLIED. STARTING GAME UNMODIFIED.");
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
        g_network_task_running = 0;
        set_details_message("COULD NOT START GAME LAUNCH WORKER. APP IS READY.");
    }
}

static void load_app_settings(void)
{
    FILE *fp = fopen(PS3SCALE_SETTINGS_PATH, "r");
    char line[64];
    int value, settings_version = 0;
    if (!fp) return;
    g_language = LANGUAGE_UNSET;
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "settings_version=%d", &value) == 1)
            settings_version = value;
        else if (sscanf(line, "music=%d", &value) == 1)
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
        else if (sscanf(line, "music_fade_ms=%d", &value) == 1 &&
                 (value == 500 || value == 1000 ||
                  value == 1500 || value == 2000))
            g_music_fade_ms = value;
        else if (sscanf(line, "language=%d", &value) == 1 &&
                 value >= LANGUAGE_ENGLISH && value <= LANGUAGE_SPANISH)
            g_language = value;
        else if (sscanf(line, "enhancement_persistent=%d", &value) == 1)
            g_enhancement_persistent = value ? 1 : 0;
    }
    fclose(fp);

    /* Alpha 0.21's old config could remain pinned to a single song. Migrate
       once to the requested alternating two-track playlist. */
    if (settings_version < 2) {
        g_music_mode = 0;
        g_music_next_track = 1;
        g_music_start_track = 1;
        g_music_fade_ms = 1000;
    }
    if (settings_version < 3)
        g_enhancement_persistent = 0;
}

static int save_app_settings(void)
{
    char contents[512];
    int length = snprintf(contents, sizeof(contents),
        "settings_version=3\n"
        "music=%d\n"
        "volume=%d\n"
        "animation_v2=%d\n"
        "music_mode=%d\n"
        "music_next=%d\n"
        "music_fade_ms=%d\n"
        "language=%d\n"
        "enhancement_persistent=%d\n",
        g_music_enabled ? 1 : 0,
        g_music_volume,
        g_animation_enabled ? 1 : 0,
        g_music_mode,
        g_music_next_track,
        g_music_fade_ms,
        g_language,
        g_enhancement_persistent ? 1 : 0);

    if (length < 0 || (size_t)length >= sizeof(contents) ||
        ps3scale_atomic_file_write(PS3SCALE_SETTINGS_PATH, contents,
                                   (size_t)length) != 0) {
        set_settings_message("SETTINGS SAVE FAILED. PREVIOUS SETTINGS KEPT.");
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

static int ps2_netemu_iso_supported(void)
{
    return g_hardware_info_valid &&
        ps3scale_hardware_supports_netemu_ps2_iso(&g_hardware_info);
}

static const char *ps2_config_preferred_type(void)
{
    if (!g_hardware_info_valid)
        return NULL;
    if (g_hardware_info.family == PS3SCALE_HARDWARE_FAT) {
        if (strstr(g_hardware_info.model_series, "CECHC") ||
            strstr(g_hardware_info.model_series, "CECHE"))
            return "GX";
        return "SOFT";
    }
    if (g_hardware_info.family == PS3SCALE_HARDWARE_SLIM ||
        g_hardware_info.family == PS3SCALE_HARDWARE_SUPER_SLIM)
        return "NET";
    return NULL;
}

static const ps3scale_ps2_config_entry_t *ps2_config_for_game(
    const ps3scale_game_t *game)
{
    const char *id;
    if (!game || !g_ps2_config_db.items)
        return NULL;

    if (game->kind == PS3SCALE_GAME_PS2_ISO) {
        const char *preferred_type = ps2_config_preferred_type();
        id = game->disc_id;
        if (!id[0] || !preferred_type)
            return NULL;
        return ps3scale_ps2_config_db_find(
            &g_ps2_config_db, id, preferred_type);
    }

    if (game->kind == PS3SCALE_GAME_PS2_CLASSIC) {
        if (game->title_id[0]) {
            const ps3scale_ps2_config_entry_t *by_title_id =
                ps3scale_ps2_config_db_find(
                    &g_ps2_config_db, game->title_id, "ENC");
            if (by_title_id)
                return by_title_id;
        }
        id = game->disc_id;
        if (!id[0])
            return NULL;
        return ps3scale_ps2_config_db_find(&g_ps2_config_db, id, "ENC");
    }
    return NULL;
}

static int ps2_config_paths_for_game(const ps3scale_game_t *game,
 char *target,size_t target_size,char *backup,size_t backup_size,char *backup_temp,size_t backup_temp_size,
 char *rollback,size_t rollback_size,char *restore_temp,size_t restore_temp_size,char *download_temp,size_t download_temp_size,size_t *max_bytes){
 int n;char normalized[PS3SCALE_PS2_CONFIG_ID_MAX];const char *id;const ps3scale_ps2_config_entry_t *entry;
 if(!game||!target||!backup||!backup_temp||!rollback||!restore_temp||!download_temp||!max_bytes)return -1;
 if(game->kind==PS3SCALE_GAME_PS2_ISO){if(!game->path[0])return -1;n=snprintf(target,target_size,"%s.CONFIG",game->path);*max_bytes=PS3SCALE_PS2_CONFIG_MAX_BYTES;}
 else if(game->kind==PS3SCALE_GAME_PS2_CLASSIC){entry=ps2_config_for_game(game);id=(entry&&strcmp(entry->type,"ENC")==0)?entry->title_id:(game->disc_id[0]?game->disc_id:game->title_id);
  if(!id[0]||ps3scale_ps2_config_normalize_id(id,normalized,sizeof(normalized))!=0)return -1;
  n=snprintf(target,target_size,"%s/%s.ENC",PS3SCALE_PS2_CLASSICS_CONFIG_DIR,normalized);*max_bytes=PS3SCALE_PS2_CONFIG_DOWNLOAD_LIMIT;}
 else return -1;
 if(n<0||(size_t)n>=target_size)return -1;
 n=snprintf(backup,backup_size,"%s.ps3scale.bak",target);if(n<0||(size_t)n>=backup_size)return -1;
 n=snprintf(backup_temp,backup_temp_size,"%s.ps3scale.backup.tmp",target);if(n<0||(size_t)n>=backup_temp_size)return -1;
 n=snprintf(rollback,rollback_size,"%s.ps3scale.rollback.bak",target);if(n<0||(size_t)n>=rollback_size)return -1;
 n=snprintf(restore_temp,restore_temp_size,"%s.ps3scale.restore.tmp",target);if(n<0||(size_t)n>=restore_temp_size)return -1;
 n=snprintf(download_temp,download_temp_size,"%s.ps3scale.tmp",target);if(n<0||(size_t)n>=download_temp_size)return -1;return 0;
}
static void refresh_ps2_config_manager_cache(void)
{
    const ps3scale_game_t *game = selected_game();

    g_ps2_config_manager_target_state = -1;
    g_ps2_config_manager_backup_state = -1;
    g_ps2_config_manager_rollback_state = -1;
    g_ps2_config_manager_target_size = 0;
    g_ps2_config_manager_backup_size = 0;
    g_ps2_config_manager_max_bytes = PS3SCALE_PS2_CONFIG_MAX_BYTES;

    if (ps2_config_paths_for_game(
            game,
            g_ps2_config_manager_target,
            sizeof(g_ps2_config_manager_target),
            g_ps2_config_manager_backup,
            sizeof(g_ps2_config_manager_backup),
            g_ps2_config_manager_backup_temp,
            sizeof(g_ps2_config_manager_backup_temp),
            g_ps2_config_manager_rollback,
            sizeof(g_ps2_config_manager_rollback),
            g_ps2_config_manager_restore_temp,
            sizeof(g_ps2_config_manager_restore_temp),
            g_ps2_config_manager_download_temp,
            sizeof(g_ps2_config_manager_download_temp),
            &g_ps2_config_manager_max_bytes) != 0) {
        g_ps2_config_manager_message[0] = '\0';
        return;
    }

    g_ps2_config_manager_target_state = ps3scale_ps2_config_store_probe(
        g_ps2_config_manager_target, g_ps2_config_manager_max_bytes,
        &g_ps2_config_manager_target_size);
    g_ps2_config_manager_backup_state = ps3scale_ps2_config_store_probe(
        g_ps2_config_manager_backup, g_ps2_config_manager_max_bytes,
        &g_ps2_config_manager_backup_size);
    g_ps2_config_manager_rollback_state = ps3scale_ps2_config_store_probe(
        g_ps2_config_manager_rollback, g_ps2_config_manager_max_bytes, NULL);
}

/*
 * Preserve the original before any replacement. A pre-existing backup is
 * accepted only when it is byte-identical to the current CONFIG.
 */
static int ps2_config_ensure_backup(const char *target, const char *backup,
                                    const char *backup_temp, size_t max_bytes)
{
    int target_state = ps3scale_ps2_config_store_probe(
        target, max_bytes, NULL);
    int backup_state;

    if (target_state == 0)
        return 0;
    if (target_state != 1)
        return -1;

    backup_state = ps3scale_ps2_config_store_probe(backup, max_bytes, NULL);
    if (backup_state < 0)
        return -2;

    if (backup_state == 0) {
        if (ps3scale_ps2_config_store_probe(
                backup_temp, max_bytes, NULL) != 0)
            return -3;
        if (ps3scale_ps2_config_store_backup(
                target, backup, backup_temp, max_bytes) != 0)
            return -4;
    }

    return ps3scale_ps2_config_store_can_replace(
        target, backup, max_bytes) == 1 ? 0 : -5;
}

static void apply_ps2_local_profile(void)
{
    uint8_t config_data[64];
    size_t config_size = 0;
    const ps3scale_game_t *game = selected_game();
    int result;
    int had_existing_config;

    if (g_network_task_running || g_ps2_config_download_running ||
        g_ps2_config_download_done || g_ps2_config_download_thread_started) {
        snprintf(g_ps2_config_manager_message,
                 sizeof(g_ps2_config_manager_message),
                 "WAIT FOR THE CURRENT PS2 CONFIG TASK TO FINISH.");
        return;
    }

    if (!game || game->kind != PS3SCALE_GAME_PS2_ISO) {
        snprintf(g_ps2_config_manager_message,
                 sizeof(g_ps2_config_manager_message),
                 "LOCAL PROFILES REQUIRE A PS2 ISO; CLASSICS NEED AN EXACT ENC CONFIG.");
        return;
    }
    if (!ps2_netemu_iso_supported()) {
        snprintf(g_ps2_config_manager_message,
                 sizeof(g_ps2_config_manager_message),
                 "LOCAL PROFILES REQUIRE CONFIRMED PS2 NETEMU HARDWARE (SLIM / SUPER SLIM).");
        return;
    }

    if (g_ps2_enhancement_flags == 0) {
        snprintf(g_ps2_config_manager_message,
                 sizeof(g_ps2_config_manager_message),
                 "SELECT AT LEAST ONE ENHANCEMENT OPTION.");
        return;
    }

    refresh_ps2_config_manager_cache();
    if (g_ps2_config_manager_target_state < 0) {
        snprintf(g_ps2_config_manager_message,
                 sizeof(g_ps2_config_manager_message),
                 "CONFIG PATH IS INVALID OR THE FILE EXCEEDS THE SAFE SIZE LIMIT.");
        return;
    }

    had_existing_config = g_ps2_config_manager_target_state == 1;
    if (had_existing_config) {
        snprintf(g_ps2_config_manager_message,
                 sizeof(g_ps2_config_manager_message),
                 "EXISTING CONFIG DETECTED. LOCAL PROFILE WILL NOT OVERWRITE IT.");
        return;
    }

    result = ps2_config_ensure_backup(
        g_ps2_config_manager_target,
        g_ps2_config_manager_backup,
        g_ps2_config_manager_backup_temp,
        g_ps2_config_manager_max_bytes);
    if (result != 0) {
        snprintf(g_ps2_config_manager_message,
                 sizeof(g_ps2_config_manager_message),
                 result == -5 ?
                 "SAVED BACKUP DOES NOT MATCH. RESTORE IT BEFORE CHANGING PROFILE." :
                 "AUTOMATIC BACKUP FAILED. NO FILE WAS CHANGED.");
        refresh_ps2_config_manager_cache();
        return;
    }

    if (ps3scale_ps2_config_build(
            config_data, sizeof(config_data),
            g_ps2_enhancement_flags, &config_size) != 0 ||
        ps3scale_ps2_config_validate_generated(
            config_data, config_size) != 0) {
        snprintf(g_ps2_config_manager_message,
                 sizeof(g_ps2_config_manager_message),
                 "CONFIG BUILD FAILED. NO FILE WAS CHANGED.");
        return;
    }

    result = ps3scale_ps2_config_store_stage(
        g_ps2_config_manager_download_temp, config_data, config_size,
        g_ps2_config_manager_max_bytes);
    if (result != 0) {
        snprintf(g_ps2_config_manager_message,
                 sizeof(g_ps2_config_manager_message),
                 "COULD NOT STAGE CONFIG. CHECK TEMP FILES AND FREE SPACE.");
        refresh_ps2_config_manager_cache();
        return;
    }

    result = ps3scale_ps2_config_store_install(
        g_ps2_config_manager_download_temp,
        g_ps2_config_manager_target,
        g_ps2_config_manager_backup,
        g_ps2_config_manager_rollback,
        g_ps2_config_manager_max_bytes);
    if (result == 0) {
        snprintf(g_ps2_config_manager_message,
                 sizeof(g_ps2_config_manager_message),
                 had_existing_config ?
                 "PS2 ENHANCEMENT CONFIG INSTALLED. ORIGINAL CONFIG BACKED UP." :
                 "PS2 ENHANCEMENT CONFIG INSTALLED. NO PREVIOUS CONFIG EXISTED.");
    } else {
        (void)unlink(g_ps2_config_manager_download_temp);
        if (result == -2)
            snprintf(g_ps2_config_manager_message,
                     sizeof(g_ps2_config_manager_message),
                     "BACKUP CHANGED. NO CONFIG WAS REPLACED.");
        else if (result == -3)
            snprintf(g_ps2_config_manager_message,
                     sizeof(g_ps2_config_manager_message),
                     "ROLLBACK FILE EXISTS OR IS INVALID. NO FILE WAS OVERWRITTEN.");
        else
            snprintf(g_ps2_config_manager_message,
                     sizeof(g_ps2_config_manager_message),
                     "CONFIG INSTALL FAILED. ORIGINAL FILE WAS PRESERVED.");
    }

    refresh_ps2_config_manager_cache();
    g_details_cache_valid = 0;
}

static void apply_ps2_game_patch(ps3scale_ps2_patch_kind_t patch_kind)
{
    const ps3scale_game_t *game = selected_game();
    const ps3scale_ps2_game_patch_t *catalog_patch;
    ps3scale_ps2_code_patch_t code_patches[PS3SCALE_PS2_CONFIG_MAX_CODE_PATCHES];
    unsigned char *elf_data = NULL;
    size_t elf_size = 0, existing_size = 0, output_size = 0, i;
    uint32_t elf_hash = 0;
    char serial[16];
    uint8_t existing_config[PS3SCALE_PS2_CONFIG_MAX_BYTES];
    uint8_t output_config[PS3SCALE_PS2_CONFIG_MAX_BYTES];
    int result;
    const int is_fps = patch_kind == PS3SCALE_PS2_PATCH_FPS;
    const int is_widescreen = patch_kind == PS3SCALE_PS2_PATCH_WIDESCREEN;

    if (g_network_task_running || g_ps2_config_download_running ||
        g_ps2_config_download_done || g_ps2_config_download_thread_started) {
        snprintf(g_ps2_config_manager_message, sizeof(g_ps2_config_manager_message),
                 "WAIT FOR THE CURRENT PS2 CONFIG TASK TO FINISH.");
        return;
    }
    if (!game || game->kind != PS3SCALE_GAME_PS2_ISO) {
        snprintf(g_ps2_config_manager_message, sizeof(g_ps2_config_manager_message),
                 "GAME PATCHES ARE AVAILABLE FOR PS2 ISO FILES ONLY.");
        return;
    }
    if (!ps2_netemu_iso_supported()) {
        snprintf(g_ps2_config_manager_message, sizeof(g_ps2_config_manager_message),
                 "GAME PATCHES REQUIRE CONFIRMED PS2 NETEMU HARDWARE (SLIM / SUPER SLIM).");
        goto cleanup;
    }
    if (ps3scale_ps2_iso_load_boot_elf(game->path, &elf_data, &elf_size,
            serial, sizeof(serial), &elf_hash) != 0) {
        snprintf(g_ps2_config_manager_message, sizeof(g_ps2_config_manager_message),
                 "PS2 BOOT ELF COULD NOT BE READ OR HASHED.");
        goto cleanup;
    }
    catalog_patch = ps3scale_ps2_game_patch_find(serial, elf_hash, patch_kind);
    if (!catalog_patch || !catalog_patch->words || !catalog_patch->word_count ||
        catalog_patch->word_count > PS3SCALE_PS2_CONFIG_MAX_CODE_PATCHES) {
        snprintf(g_ps2_config_manager_message, sizeof(g_ps2_config_manager_message),
                 is_fps ?
                 "NO EXACT 60 FPS PATCH MATCH FOR THIS GAME SERIAL / ELF HASH." :
                 (is_widescreen ?
                  "NO EXACT 16:9 PATCH MATCH FOR THIS GAME SERIAL / ELF HASH." :
                  "NO EXACT 480P PATCH MATCH FOR THIS GAME SERIAL / ELF HASH."));
        goto cleanup;
    }
    for (i = 0; i < catalog_patch->word_count; ++i) {
        uint32_t original_word = 0;
        if (ps3scale_ps2_elf_read_exec_word(elf_data, elf_size,
                catalog_patch->words[i].ee_address, &original_word) != 0) {
            snprintf(g_ps2_config_manager_message, sizeof(g_ps2_config_manager_message),
                     "PATCH ADDRESS IS NOT IN A FILE-BACKED EXECUTABLE ELF SEGMENT.");
            goto cleanup;
        }
        code_patches[i].ee_address = catalog_patch->words[i].ee_address;
        code_patches[i].original_word = original_word;
        code_patches[i].replacement_word = catalog_patch->words[i].replacement_word;
    }
    refresh_ps2_config_manager_cache();
    if (g_ps2_config_manager_target_state < 0) {
        snprintf(g_ps2_config_manager_message, sizeof(g_ps2_config_manager_message),
                 "CONFIG PATH IS INVALID OR THE FILE EXCEEDS THE SAFE SIZE LIMIT.");
        goto cleanup;
    }
    if (g_ps2_config_manager_target_state == 1) {
        result = ps3scale_ps2_config_store_read(g_ps2_config_manager_target,
            existing_config, sizeof(existing_config),
            g_ps2_config_manager_max_bytes, &existing_size);
        if (result != 0 || ps3scale_ps2_config_validate_external(
                existing_config, existing_size, "CUSTOM") != 0) {
            snprintf(g_ps2_config_manager_message, sizeof(g_ps2_config_manager_message),
                     "EXISTING CONFIG FORMAT CANNOT BE MERGED SAFELY.");
            goto cleanup;
        }
        result = ps3scale_ps2_config_append_patches(existing_config, existing_size,
            code_patches, catalog_patch->word_count, output_config,
            sizeof(output_config), &output_size);
        if (result == -2) {
            snprintf(g_ps2_config_manager_message, sizeof(g_ps2_config_manager_message),
                     "TITLE ID / MULTIDISC CONFIG CANNOT BE REORDERED SAFELY.");
            goto cleanup;
        }
        if (result == -3) {
            snprintf(g_ps2_config_manager_message, sizeof(g_ps2_config_manager_message),
                     "CONFIG HAS A DUPLICATE OR UNSUPPORTED EE PATCH. NO FILE WAS CHANGED.");
            goto cleanup;
        }
        if (result != 0) {
            snprintf(g_ps2_config_manager_message, sizeof(g_ps2_config_manager_message),
                     "CONFIG CANNOT BE MERGED SAFELY. NO FILE WAS CHANGED.");
            goto cleanup;
        }
    } else if (ps3scale_ps2_config_build_with_patches(output_config,
            sizeof(output_config), 0, code_patches, catalog_patch->word_count,
            &output_size) != 0 ||
        ps3scale_ps2_config_validate_generated(output_config, output_size) != 0) {
        snprintf(g_ps2_config_manager_message, sizeof(g_ps2_config_manager_message),
                 "GAME PATCH CONFIG BUILD FAILED. NO FILE WAS CHANGED.");
        goto cleanup;
    }
    result = ps2_config_ensure_backup(g_ps2_config_manager_target,
        g_ps2_config_manager_backup, g_ps2_config_manager_backup_temp,
        g_ps2_config_manager_max_bytes);
    if (result != 0) {
        snprintf(g_ps2_config_manager_message, sizeof(g_ps2_config_manager_message),
                 result == -5 ?
                 "SAVED BACKUP DOES NOT MATCH. RESTORE IT BEFORE CHANGING PROFILE." :
                 "AUTOMATIC BACKUP FAILED. NO FILE WAS CHANGED.");
        goto cleanup;
    }
    result = ps3scale_ps2_config_store_stage(g_ps2_config_manager_download_temp,
        output_config, output_size, g_ps2_config_manager_max_bytes);
    if (result != 0) {
        snprintf(g_ps2_config_manager_message, sizeof(g_ps2_config_manager_message),
                 "COULD NOT STAGE GAME PATCH CONFIG. CHECK TEMP FILES.");
        goto cleanup;
    }
    result = ps3scale_ps2_config_store_install(g_ps2_config_manager_download_temp,
        g_ps2_config_manager_target, g_ps2_config_manager_backup,
        g_ps2_config_manager_rollback, g_ps2_config_manager_max_bytes);
    if (result == 0) {
        snprintf(g_ps2_config_manager_message, sizeof(g_ps2_config_manager_message),
                 is_fps ?
                 "EXPERIMENTAL FPS PATCH INSTALLED. ORIGINAL ISO WAS NOT CHANGED." :
                 (is_widescreen ?
                  "EXPERIMENTAL 16:9 PATCH INSTALLED. ORIGINAL ISO WAS NOT CHANGED." :
                  "EXPERIMENTAL 480P PATCH INSTALLED. ORIGINAL ISO WAS NOT CHANGED."));
        g_details_cache_valid = 0;
    } else {
        (void)unlink(g_ps2_config_manager_download_temp);
        snprintf(g_ps2_config_manager_message, sizeof(g_ps2_config_manager_message),
                 "GAME PATCH INSTALL FAILED. ORIGINAL CONFIG WAS PRESERVED.");
    }
cleanup:
    free(elf_data);
    refresh_ps2_config_manager_cache();
}

static int ps2_game_patch_is_available(ps3scale_ps2_patch_kind_t patch_kind)
{
    if (patch_kind == PS3SCALE_PS2_PATCH_FPS)
        return g_ps2_patch_fps_available;
    if (patch_kind == PS3SCALE_PS2_PATCH_WIDESCREEN)
        return g_ps2_patch_widescreen_available;
    if (patch_kind == PS3SCALE_PS2_PATCH_480P)
        return g_ps2_patch_480p_available;
    return 0;
}

/*
 * Code patches alter the PS2 EE instruction stream. The first CROSS only
 * arms the exact, already-probed candidate; a second CROSS applies it.
 * Navigation or CIRCLE cancels the pending action without touching CONFIG.
 */
static void request_ps2_game_patch(ps3scale_ps2_patch_kind_t patch_kind)
{
    const char *label =
        patch_kind == PS3SCALE_PS2_PATCH_FPS ? "60 FPS" :
        (patch_kind == PS3SCALE_PS2_PATCH_WIDESCREEN ? "16:9" : "480P");

    if (g_ps2_patch_confirm_pending) {
        if (g_ps2_patch_confirm_kind != patch_kind) {
            g_ps2_patch_confirm_pending = 0;
            snprintf(g_ps2_config_manager_message,
                     sizeof(g_ps2_config_manager_message),
                     "PATCH CONFIRMATION CANCELLED. NO FILE WAS CHANGED.");
            return;
        }
        g_ps2_patch_confirm_pending = 0;
        apply_ps2_game_patch(patch_kind);
        return;
    }

    /* Let the existing validation path explain unsupported / unmatched cases. */
    if (!ps2_game_patch_is_available(patch_kind)) {
        apply_ps2_game_patch(patch_kind);
        return;
    }

    g_ps2_patch_confirm_pending = 1;
    g_ps2_patch_confirm_kind = patch_kind;
    snprintf(g_ps2_config_manager_message,
             sizeof(g_ps2_config_manager_message),
             "REVIEW %s PATCH. PRESS CROSS AGAIN TO APPLY; CIRCLE CANCELS.",
             label);
}

static void ps2_config_download_worker(void *arg)
{
    unsigned char *data = NULL;
    FILE *fp = NULL;
    long file_size = 0;
    size_t max_bytes;
    int install_result;
    int result = -1;
    int downloaded = 0;

    (void)arg;
    if (ps3scale_online_download_limited_progress(
            g_ps2_config_download_url,
            g_ps2_config_download_temp_path,
            PS3SCALE_PS2_CONFIG_DOWNLOAD_LIMIT,
            &g_ps2_config_downloaded, &g_ps2_config_download_total) == 0) {
        downloaded = 1;
        fp = fopen(g_ps2_config_download_temp_path, "rb");
        if (!fp) {
            result = -3;
        } else {
            if (fseek(fp, 0, SEEK_END) == 0)
                file_size = ftell(fp);
            if (file_size <= 0 ||
                (size_t)file_size > PS3SCALE_PS2_CONFIG_DOWNLOAD_LIMIT ||
                fseek(fp, 0, SEEK_SET) != 0) {
                result = -3;
            } else {
                data = (unsigned char *)malloc((size_t)file_size);
                if (!data ||
                    fread(data, 1, (size_t)file_size, fp) != (size_t)file_size ||
                    ferror(fp)) {
                    result = -3;
                } else if (ps3scale_ps2_config_validate_external(
                               data, (size_t)file_size,
                               g_ps2_config_download_type) != 0) {
                    result = -4;
                } else {
                    max_bytes = strcmp(g_ps2_config_download_type, "ENC") == 0 ?
                        PS3SCALE_PS2_CONFIG_DOWNLOAD_LIMIT : PS3SCALE_PS2_CONFIG_MAX_BYTES;
                    install_result = ps3scale_ps2_config_store_install(
                        g_ps2_config_download_temp_path,g_ps2_config_download_path,
                        g_ps2_config_download_backup_path,g_ps2_config_download_rollback_path,max_bytes);
                    if (install_result == 0) result = 0;
                    else if (install_result == -2) result = -2;
                    else if (install_result == -3) result = -6;
                    else result = -5;
                }
            }
            free(data);
            data = NULL;
            fclose(fp);
            fp = NULL;
        }
    }

    if (result != 0 && downloaded)
        (void)unlink(g_ps2_config_download_temp_path);
    g_ps2_config_download_result = result;
    g_ps2_config_download_running = 0;
    g_ps2_config_download_done = 1;
    g_network_task_running = 0;
}

static void start_ps2_config_download(void){
 const ps3scale_game_t *game=selected_game();const ps3scale_ps2_config_entry_t *entry=ps2_config_for_game(game);
 char url[512],target[PS3SCALE_PATH_MAX],backup[PS3SCALE_PATH_MAX],backup_temp[PS3SCALE_PATH_MAX],rollback[PS3SCALE_PATH_MAX],restore_temp[PS3SCALE_PATH_MAX],temporary[PS3SCALE_PATH_MAX];
 size_t max_bytes=PS3SCALE_PS2_CONFIG_MAX_BYTES;int state;
 if(g_network_task_running||g_ps2_config_download_running||g_ps2_config_download_done||g_ps2_config_download_thread_started){set_details_message("WAIT FOR THE CURRENT PS2 CONFIG TASK TO FINISH.");goto copy_message;}
 if(!game||!entry){set_details_message("NO VERIFIED PS2 CONFIG MATCHES THIS GAME ID.");goto copy_message;}
 if(ps3scale_ps2_config_build_url(entry,url,sizeof(url))!=0){set_details_message("PS2 CONFIG SOURCE URL IS INVALID.");goto copy_message;}
 if((strcmp(entry->type,"ENC")==0&&game->kind!=PS3SCALE_GAME_PS2_CLASSIC)||(strcmp(entry->type,"ENC")!=0&&game->kind!=PS3SCALE_GAME_PS2_ISO)){set_details_message("CONFIG TYPE DOES NOT MATCH THIS PS2 GAME FORMAT.");goto copy_message;}
 if(ps2_config_paths_for_game(game,target,sizeof(target),backup,sizeof(backup),backup_temp,sizeof(backup_temp),rollback,sizeof(rollback),restore_temp,sizeof(restore_temp),temporary,sizeof(temporary),&max_bytes)!=0){set_details_message("PS2 CONFIG TARGET PATH IS INVALID OR TOO LONG.");goto copy_message;}
 if(strcmp(entry->type,"ENC")==0){struct stat st;if(stat(PS3SCALE_PS2_CLASSICS_CONFIG_DIR,&st)!=0||!S_ISDIR(st.st_mode)){set_details_message("INSTALL THE WEBMAN PS2CONFIG PACKAGE FOR ENC FILES.");goto copy_message;}}
 state=ps3scale_ps2_config_store_probe(target,max_bytes,NULL);
 if(state<0){set_details_message("EXISTING CONFIG IS INVALID OR EXCEEDS THE SAFE SIZE LIMIT.");goto copy_message;}
 if(ps3scale_ps2_config_store_probe(temporary,max_bytes,NULL)!=0){set_details_message("A PS3 SCALE TEMP FILE ALREADY EXISTS. CHECK THE CONFIG FOLDER.");goto copy_message;}
 if(state==1&&ps2_config_ensure_backup(target,backup,backup_temp,max_bytes)!=0){set_details_message("SAVED BACKUP DOES NOT MATCH. RESTORE IT BEFORE REPLACING CONFIG.");goto copy_message;}
 snprintf(g_ps2_config_download_url,sizeof(g_ps2_config_download_url),"%s",url);
 snprintf(g_ps2_config_download_path,sizeof(g_ps2_config_download_path),"%s",target);
 snprintf(g_ps2_config_download_temp_path,sizeof(g_ps2_config_download_temp_path),"%s",temporary);
 snprintf(g_ps2_config_download_backup_path,sizeof(g_ps2_config_download_backup_path),"%s",backup);
 snprintf(g_ps2_config_download_rollback_path,sizeof(g_ps2_config_download_rollback_path),"%s",rollback);
 snprintf(g_ps2_config_download_type,sizeof(g_ps2_config_download_type),"%s",entry->type);
 g_ps2_config_download_result=-1;g_ps2_config_downloaded=0;g_ps2_config_download_total=0;g_ps2_config_download_done=0;g_ps2_config_download_running=1;g_network_task_running=1;
 if(sysThreadCreate(&g_ps2_config_download_thread,ps2_config_download_worker,NULL,1001,64*1024,THREAD_JOINABLE,"ps3scale_ps2cfg")!=0){
  g_ps2_config_download_running=0;g_ps2_config_downloaded=0;g_ps2_config_download_total=0;g_network_task_running=0;set_details_message("COULD NOT START PS2 CONFIG DOWNLOAD WORKER.");goto copy_message;}
 g_ps2_config_download_thread_started=1;set_details_message("DOWNLOADING VERIFIED PS2 CONFIG. ORIGINAL GAME FILE IS UNCHANGED.");
copy_message:
 snprintf(g_ps2_config_manager_message,sizeof(g_ps2_config_manager_message),"%s",g_details_message);
}
static void perform_ps2_config_manager_action(void)
{
    int result;
    const ps3scale_game_t *game = selected_game();
    const ps3scale_ps2_config_entry_t *entry = ps2_config_for_game(game);

    if (g_network_task_running || g_ps2_config_download_running ||
        g_ps2_config_download_done || g_ps2_config_download_thread_started) {
        snprintf(g_ps2_config_manager_message,
                 sizeof(g_ps2_config_manager_message),
                 "WAIT FOR THE CURRENT PS2 CONFIG TASK TO FINISH.");
        return;
    }

    refresh_ps2_config_manager_cache();
    if (g_ps2_config_manager_target_state < 0) {
        snprintf(g_ps2_config_manager_message,
                 sizeof(g_ps2_config_manager_message),
                 "CONFIG PATH IS INVALID OR THE FILE EXCEEDS THE SAFE SIZE LIMIT.");
        return;
    }

    if (g_ps2_config_manager_selection <= 3) {
        unsigned flag = g_ps2_config_manager_selection == 0 ?
            PS3SCALE_PS2_CFG_ENABLE_L2H :
            (g_ps2_config_manager_selection == 1 ?
             PS3SCALE_PS2_CFG_DISABLE_SMOOTHING :
             (g_ps2_config_manager_selection == 2 ?
              PS3SCALE_PS2_CFG_FRAME_PACING :
              PS3SCALE_PS2_CFG_ENABLE_PRESSURE));

        if (!game || game->kind != PS3SCALE_GAME_PS2_ISO) {
            snprintf(g_ps2_config_manager_message,
                     sizeof(g_ps2_config_manager_message),
                     "LOCAL PROFILES REQUIRE A PS2 ISO; CLASSICS NEED AN EXACT ENC CONFIG.");
            return;
        }
        if (!ps2_netemu_iso_supported()) {
            snprintf(g_ps2_config_manager_message,
                     sizeof(g_ps2_config_manager_message),
                     "NETEMU OPTIONS ARE DISABLED: CONFIRM A PS3 SLIM / SUPER SLIM FIRST.");
            return;
        }

        g_ps2_enhancement_flags ^= flag;
        if (flag == PS3SCALE_PS2_CFG_ENABLE_L2H)
            snprintf(g_ps2_config_manager_message,
                     sizeof(g_ps2_config_manager_message),
                     (g_ps2_enhancement_flags & flag) ?
                     "L2H PERFORMANCE ENABLED." : "L2H PERFORMANCE DISABLED.");
        else if (flag == PS3SCALE_PS2_CFG_DISABLE_SMOOTHING)
            snprintf(g_ps2_config_manager_message,
                     sizeof(g_ps2_config_manager_message),
                     (g_ps2_enhancement_flags & flag) ?
                     "SHARPER PIXELS ENABLED." : "SHARPER PIXELS DISABLED.");
        else if (flag == PS3SCALE_PS2_CFG_FRAME_PACING)
            snprintf(g_ps2_config_manager_message,
                     sizeof(g_ps2_config_manager_message),
                     (g_ps2_enhancement_flags & flag) ?
                     "FRAME PACING ENABLED (EXPERIMENTAL)." :
                     "FRAME PACING DISABLED.");
        else
            snprintf(g_ps2_config_manager_message,
                     sizeof(g_ps2_config_manager_message),
                     (g_ps2_enhancement_flags & flag) ?
                     "PRESSURE SENSITIVITY ENABLED." :
                     "PRESSURE SENSITIVITY DISABLED.");
        return;
    }

    if (g_ps2_config_manager_selection == 4) {
        apply_ps2_local_profile();
        return;
    }

    if (g_ps2_config_manager_selection == 5) {
        request_ps2_game_patch(PS3SCALE_PS2_PATCH_FPS);
        return;
    }

    if (g_ps2_config_manager_selection == 6) {
        request_ps2_game_patch(PS3SCALE_PS2_PATCH_WIDESCREEN);
        return;
    }

    if (g_ps2_config_manager_selection == 7) {
        request_ps2_game_patch(PS3SCALE_PS2_PATCH_480P);
        return;
    }

    if (g_ps2_config_manager_selection == 8) {
        if (!entry) {
            snprintf(g_ps2_config_manager_message,
                     sizeof(g_ps2_config_manager_message),
                     "NO EXACT PS2 CONFIG MATCH IS AVAILABLE TO DOWNLOAD.");
        } else {
            start_ps2_config_download();
        }
        refresh_ps2_config_manager_cache();
        return;
    }

    if (g_ps2_config_manager_selection == 9) {
        result = ps3scale_ps2_config_store_backup(
            g_ps2_config_manager_target,
            g_ps2_config_manager_backup,
            g_ps2_config_manager_backup_temp,
            g_ps2_config_manager_max_bytes);
        if (result == 0)
            snprintf(g_ps2_config_manager_message,
                     sizeof(g_ps2_config_manager_message),
                     "BACKUP CREATED AND VERIFIED BYTE-FOR-BYTE.");
        else if (result == -2)
            snprintf(g_ps2_config_manager_message,
                     sizeof(g_ps2_config_manager_message),
                     "BACKUP ALREADY EXISTS. IT WAS NOT OVERWRITTEN.");
        else if (result == -4)
            snprintf(g_ps2_config_manager_message,
                     sizeof(g_ps2_config_manager_message),
                     "NO VALID CONFIG FILE EXISTS TO BACK UP.");
        else
            snprintf(g_ps2_config_manager_message,
                     sizeof(g_ps2_config_manager_message),
                     "BACKUP FAILED. NO ORIGINAL FILE WAS CHANGED.");
    } else if (g_ps2_config_manager_selection == 10) {
        result = ps3scale_ps2_config_store_restore(
            g_ps2_config_manager_target,
            g_ps2_config_manager_backup,
            g_ps2_config_manager_rollback,
            g_ps2_config_manager_restore_temp,
            g_ps2_config_manager_max_bytes);
        if (result == 0)
            snprintf(g_ps2_config_manager_message,
                     sizeof(g_ps2_config_manager_message),
                     "BACKUP RESTORED. PREVIOUS CONFIG KEPT AS ROLLBACK.");
        else if (result == 1)
            snprintf(g_ps2_config_manager_message,
                     sizeof(g_ps2_config_manager_message),
                     "CURRENT CONFIG ALREADY MATCHES THE SAVED BACKUP.");
        else if (result == -2)
            snprintf(g_ps2_config_manager_message,
                     sizeof(g_ps2_config_manager_message),
                     "NO VALID BACKUP EXISTS FOR THIS GAME.");
        else if (result == -3)
            snprintf(g_ps2_config_manager_message,
                     sizeof(g_ps2_config_manager_message),
                     "ROLLBACK OR TEMP FILE EXISTS. NO FILE WAS OVERWRITTEN.");
        else
            snprintf(g_ps2_config_manager_message,
                     sizeof(g_ps2_config_manager_message),
                     "RESTORE FAILED. THE CURRENT CONFIG WAS PRESERVED.");
    }

    refresh_ps2_config_manager_cache();
    g_details_cache_valid = 0;
}

static int community_extension_is(const char *extension, const char *expected)
{
    while (*extension && *expected) {
        if (tolower((unsigned char)*extension) !=
            tolower((unsigned char)*expected))
            return 0;
        extension++;
        expected++;
    }
    return *extension == '\0' && *expected == '\0';
}

static int community_component_is_safe(const char *value, int allow_slash)
{
    const unsigned char *p = (const unsigned char *)value;
    if (!value || !value[0]) return 0;
    if (strstr(value, "..") || strchr(value, '\\')) return 0;
    if (value[0] == '/' || value[strlen(value) - 1] == '/') return 0;
    for (; *p; p++) {
        if (isalnum(*p) || *p == '-' || *p == '_' || *p == '.' ||
            *p == '~' || *p == ' ' || *p == '(' || *p == ')' ||
            *p == '[' || *p == ']')
            continue;
        if (allow_slash && *p == '/') continue;
        return 0;
    }
    return 1;
}

static int community_url_encode(char *out, size_t capacity,
                                const char *input, int keep_slash)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t used = 0;
    const unsigned char *p = (const unsigned char *)input;
    if (!out || !capacity || !input) return -1;
    while (*p) {
        unsigned char c = *p++;
        int safe = isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' ||
                   (keep_slash && c == '/');
        if (safe) {
            if (used + 1 >= capacity) return -1;
            out[used++] = (char)c;
        } else {
            if (used + 3 >= capacity) return -1;
            out[used++] = '%';
            out[used++] = hex[(c >> 4) & 15];
            out[used++] = hex[c & 15];
        }
    }
    out[used] = '\0';
    return 0;
}

static int is_ps3scale_temp_enhancement_name(const char *name)
{
    size_t length, i;
    if (!name)
        return 0;
    length = strlen(name);
    if (length < 14 || strncmp(name, "PS3SCALE_", 9) != 0 ||
        !community_extension_is(name + length - 4, ".ncl"))
        return 0;
    for (i = 0; i < length; i++) {
        unsigned char ch = (unsigned char)name[i];
        if (!(isalnum(ch) || ch == '_' || ch == '-' || ch == '.'))
            return 0;
    }
    return 1;
}

static void cleanup_temporary_enhancements(void)
{
    FILE *manifest = fopen(PS3SCALE_TEMP_ENHANCEMENT_LIST_PATH, "r");
    char name[192];
    if (!manifest)
        return;

    while (fgets(name, sizeof(name), manifest)) {
        char target[PS3SCALE_PATH_MAX];
        size_t length = strcspn(name, "\r\n");
        int written;
        name[length] = '\0';
        if (!is_ps3scale_temp_enhancement_name(name))
            continue;
        written = snprintf(target, sizeof(target), "%s/%s",
                           PS3SCALE_ARTEMIS_USERLIST_DIR, name);
        if (written > 0 && (size_t)written < sizeof(target))
            (void)unlink(target);
        written = snprintf(target, sizeof(target), "%s/%s",
                           PS3SCALE_ARTEMIS_R5_USERLIST_DIR, name);
        if (written > 0 && (size_t)written < sizeof(target))
            (void)unlink(target);
    }
    fclose(manifest);
    (void)unlink(PS3SCALE_TEMP_ENHANCEMENT_LIST_PATH);
}

static int record_temporary_enhancement(const char *path)
{
    const char *name;
    FILE *manifest;
    int failed;
    if (!path || !path[0])
        return -1;
    name = strrchr(path, '/');
    name = name ? name + 1 : path;
    if (!is_ps3scale_temp_enhancement_name(name))
        return -1;
    manifest = fopen(PS3SCALE_TEMP_ENHANCEMENT_LIST_PATH, "a");
    if (!manifest)
        return -1;
    failed = fprintf(manifest, "%s\n", name) < 0;
    if (fclose(manifest) != 0)
        failed = 1;
    return failed ? -1 : 0;
}

static void community_download_worker(void *arg)
{
    FILE *fp = NULL;
    unsigned char header[256];
    unsigned char block[512];
    size_t got = 0;
    long file_size = -1;
    int result = -1;
    int downloaded = 0;
    struct stat st;
    (void)arg;

    if (ps3scale_online_download_limited_progress(
            g_community_download_url,
            g_community_download_temp_path,
            PS3SCALE_COMMUNITY_INDEX_DOWNLOAD_LIMIT,
            &g_community_downloaded, &g_community_download_total) == 0) {
        downloaded = 1;
        fp = fopen(g_community_download_temp_path, "rb");
        if (!fp) {
            result = -3;
        } else {
            if (fseek(fp, 0, SEEK_END) == 0)
                file_size = ftell(fp);
            if (file_size <= 0 ||
                (unsigned long)file_size > PS3SCALE_COMMUNITY_INDEX_DOWNLOAD_LIMIT ||
                fseek(fp, 0, SEEK_SET) != 0) {
                result = -3;
            } else if (g_community_download_expected_size == 0 ||
                       (unsigned long)file_size !=
                           g_community_download_expected_size) {
                /* The index is the identity/size snapshot for this file.
                   Reject a changed upstream file instead of installing it. */
                result = -6;
            } else {
                int text_file = 1;
                got = fread(header, 1, sizeof(header), fp);
                if (ferror(fp) || got == 0 ||
                    (got >= 4 && header[0] == 0x7f &&
                     header[1] == 'E' && header[2] == 'L' && header[3] == 'F') ||
                    (got >= 4 && header[0] == 0x7f &&
                     header[1] == 'P' && header[2] == 'K' && header[3] == 'G') ||
                    memchr(header, 0, got) != NULL) {
                    text_file = 0;
                }
                if (text_file && fseek(fp, 0, SEEK_SET) != 0)
                    text_file = 0;
                while (text_file) {
                    size_t i;
                    got = fread(block, 1, sizeof(block), fp);
                    for (i = 0; i < got; i++) {
                        unsigned char ch = block[i];
                        if (ch == 0 ||
                            (ch < 0x20u && ch != '\n' &&
                             ch != '\r' && ch != '\t')) {
                            text_file = 0;
                            break;
                        }
                    }
                    if (!text_file || got < sizeof(block))
                        break;
                }
                if (!text_file || ferror(fp)) {
                    result = -4;
                } else if (ps3scale_community_verify_git_blob_sha1(
                               g_community_download_temp_path,
                               g_community_download_expected_size,
                               g_community_download_expected_sha1) != 1) {
                    result = -7;
                } else if (stat(g_community_download_path, &st) == 0) {
                    result = -2;
                } else if (rename(g_community_download_temp_path,
                                  g_community_download_path) == 0) {
                    result = 0;
                } else {
                    result = -5;
                }
            }
            fclose(fp);
            fp = NULL;
        }
    }

    if (result != 0 && downloaded)
        (void)unlink(g_community_download_temp_path);
    g_community_download_result = result;
    g_community_download_running = 0;
    g_community_download_done = 1;
    g_network_task_running = 0;
}

static void start_community_file_download(
    const ps3scale_community_file_t *entry,
    const ps3scale_game_t *game)
{
    char download_dir[PS3SCALE_PATH_MAX];
    struct stat st;
    char encoded_branch[192];
    char encoded_path[640];
    char safe_base[96];
    char safe_version[12];
    char filename[192];
    const char *base;
    const char *dot;
    size_t i, n;
    int written;

    if (!entry) {
        snprintf(g_online_message, sizeof(g_online_message),
                 "SELECT A COMMUNITY FILE FIRST.");
        return;
    }
    if (!entry->git_blob_sha1[0]) {
        snprintf(g_online_message, sizeof(g_online_message),
                 "SOURCE HASH MISSING. REFRESH THE COMMUNITY INDEX.");
        return;
    }
    if (!game || !game_is_ps3(game) ||
        strncmp(game->path, "/dev_hdd0/", 10) != 0 ||
        !game->title_id[0] || !game->app_ver[0] ||
        strlen(game->title_id) != 9 ||
        !ps3scale_community_file_matches_exact_game(entry, game)) {
        snprintf(g_online_message, sizeof(g_online_message),
                 "INSTALL THE EXACT HDD GAME VERSION AND RESCAN FIRST.");
        return;
    }
    for (i = 0; i < 9; i++) {
        if (!isalnum((unsigned char)game->title_id[i])) {
            snprintf(g_online_message, sizeof(g_online_message),
                     "GAME TITLE ID IS INVALID. RESCAN THE INSTALLED GAME.");
            return;
        }
    }
    if (stat(game->path, &st) != 0) {
        snprintf(g_online_message, sizeof(g_online_message),
                 "GAME PATH NOT FOUND. RESCAN THE INSTALLED LIBRARY.");
        return;
    }
    if (strcmp(entry->category, "GRAPHICS_FPS") != 0) {
        snprintf(g_online_message, sizeof(g_online_message),
                 "THIS NCL FILE IS NOT A GRAPHICS / FPS ENHANCEMENT.");
        return;
    }
    if (file_exists_readonly(PS3SCALE_ARTEMIS_EBOOT_PATH)) {
        snprintf(download_dir, sizeof(download_dir), "%s",
                 PS3SCALE_ARTEMIS_USERLIST_DIR);
    } else if (file_exists_readonly(PS3SCALE_ARTEMIS_R5_EBOOT_PATH)) {
        snprintf(download_dir, sizeof(download_dir), "%s",
                 PS3SCALE_ARTEMIS_R5_USERLIST_DIR);
    } else {
        snprintf(g_online_message, sizeof(g_online_message),
                 "INSTALL ARTEMIS PS3 R5 OR THE OFFICIAL BUILD FIRST.");
        return;
    }
    if (g_network_task_running ||
        g_community_download_running || g_community_download_done ||
        g_community_download_thread_started ||
        g_catalog_sync_running || g_catalog_sync_done ||
        g_ps2_config_download_running || g_ps2_config_download_done) {
        snprintf(g_online_message, sizeof(g_online_message),
                 "WAIT FOR THE CURRENT ONLINE TASK TO FINISH.");
        return;
    }
    if (!community_component_is_safe(entry->repository, 1) ||
        !community_component_is_safe(entry->branch, 1) ||
        !community_component_is_safe(entry->path, 1) ||
        strchr(entry->repository, '/') == NULL ||
        strchr(strchr(entry->repository, '/') + 1, '/') != NULL) {
        snprintf(g_online_message, sizeof(g_online_message),
                 "DOWNLOAD REJECTED: UNSAFE GITHUB PATH.");
        return;
    }
    if (entry->file_size == 0 ||
        entry->file_size > PS3SCALE_COMMUNITY_INDEX_DOWNLOAD_LIMIT) {
        snprintf(g_online_message, sizeof(g_online_message),
                 "DOWNLOAD REJECTED: INVALID OR OVERSIZED FILE.");
        return;
    }
    dot = strrchr(entry->path, '.');
    if (!dot || !community_extension_is(dot, ".ncl")) {
        snprintf(g_online_message, sizeof(g_online_message),
                 "ONLY VERIFIED NCL GRAPHICS PATCHES CAN BE INSTALLED.");
        return;
    }
    if (community_url_encode(encoded_branch, sizeof(encoded_branch),
                             entry->branch, 1) != 0 ||
        community_url_encode(encoded_path, sizeof(encoded_path),
                             entry->path, 1) != 0) {
        snprintf(g_online_message, sizeof(g_online_message),
                 "DOWNLOAD REJECTED: GITHUB PATH IS TOO LONG.");
        return;
    }
    written = snprintf(g_community_download_url,
                       sizeof(g_community_download_url),
                       "https://raw.githubusercontent.com/%s/%s/%s",
                       entry->repository, encoded_branch, encoded_path);
    if (written < 0 || (size_t)written >= sizeof(g_community_download_url)) {
        snprintf(g_online_message, sizeof(g_online_message),
                 "DOWNLOAD REJECTED: SOURCE URL IS TOO LONG.");
        return;
    }

    base = strrchr(entry->path, '/');
    base = base ? base + 1 : entry->path;
    n = 0;
    for (i = 0; base[i] && n + 1 < sizeof(safe_base); i++) {
        unsigned char ch = (unsigned char)base[i];
        safe_base[n++] = (char)(isalnum(ch) || ch == '.' ||
                                ch == '_' || ch == '-' ? ch : '_');
    }
    safe_base[n] = '\0';
    if (!safe_base[0]) {
        snprintf(g_online_message, sizeof(g_online_message),
                 "DOWNLOAD REJECTED: EMPTY PATCH FILE NAME.");
        return;
    }
    {
        size_t base_name_length = strlen(safe_base);
        if (base_name_length >= 4 &&
            community_extension_is(safe_base + base_name_length - 4, ".ncl")) {
            memcpy(safe_base + base_name_length - 4, ".ncl", 5);
        } else if (base_name_length + 4 < sizeof(safe_base)) {
            memcpy(safe_base + base_name_length, ".ncl", 5);
        } else {
            snprintf(g_online_message, sizeof(g_online_message),
                     "DOWNLOAD REJECTED: PATCH FILE NAME IS TOO LONG.");
            return;
        }
    }

    n = 0;
    for (i = 0; game->app_ver[i] && n + 1 < sizeof(safe_version); i++) {
        unsigned char ch = (unsigned char)game->app_ver[i];
        safe_version[n++] = (char)(isalnum(ch) || ch == '.' ||
                                   ch == '-' ? ch : '_');
    }
    safe_version[n] = '\0';
    if (!safe_version[0]) {
        snprintf(g_online_message, sizeof(g_online_message),
                 "DOWNLOAD REJECTED: INVALID GAME VERSION.");
        return;
    }

    written = snprintf(filename, sizeof(filename), "PS3SCALE_%s_%s_%s",
                       game->title_id, safe_version, safe_base);
    if (written < 0 || (size_t)written >= sizeof(filename)) {
        snprintf(g_online_message, sizeof(g_online_message),
                 "DOWNLOAD REJECTED: PATCH FILE NAME IS TOO LONG.");
        return;
    }

    if (mkdir(download_dir, 0777) != 0 &&
        (stat(download_dir, &st) != 0 || !S_ISDIR(st.st_mode))) {
        snprintf(g_online_message, sizeof(g_online_message),
                 "ARTEMIS USERLIST FOLDER COULD NOT BE CREATED.");
        return;
    }
    written = snprintf(g_community_download_path,
                       sizeof(g_community_download_path),
                       "%s/%s", download_dir, filename);
    if (written < 0 || (size_t)written >= sizeof(g_community_download_path)) {
        snprintf(g_online_message, sizeof(g_online_message),
                 "DOWNLOAD REJECTED: DESTINATION PATH IS TOO LONG.");
        return;
    }
    if (stat(g_community_download_path, &st) == 0) {
        snprintf(g_online_message, sizeof(g_online_message),
                 "PATCH FILE ALREADY EXISTS IN ARTEMIS USERLIST.");
        return;
    }
    written = snprintf(g_community_download_temp_path,
                       sizeof(g_community_download_temp_path),
                       "%s.ps3scale.tmp", g_community_download_path);
    if (written < 0 || (size_t)written >= sizeof(g_community_download_temp_path)) {
        snprintf(g_online_message, sizeof(g_online_message),
                 "DOWNLOAD REJECTED: TEMPORARY PATH IS TOO LONG.");
        return;
    }

    g_community_download_persistent = g_enhancement_persistent;
    g_community_download_expected_size = entry->file_size;
    snprintf(g_community_download_expected_sha1,
             sizeof(g_community_download_expected_sha1), "%s",
             entry->git_blob_sha1);
    g_community_download_result = -1;
    g_community_downloaded = 0;
    g_community_download_total = 0;
    g_community_download_done = 0;
    g_community_download_running = 1;
    g_network_task_running = 1;
    if (sysThreadCreate(&g_community_download_thread,
                        community_download_worker, NULL,
                        1001, 64 * 1024, THREAD_JOINABLE,
                        "ps3scale_community") != 0) {
        g_community_download_running = 0;
        g_community_downloaded = 0;
        g_community_download_total = 0;
        g_community_download_expected_size = 0;
        g_community_download_expected_sha1[0] = '\0';
        g_network_task_running = 0;
        snprintf(g_online_message, sizeof(g_online_message),
                 "COULD NOT START COMMUNITY DOWNLOAD WORKER.");
        return;
    }
    g_community_download_thread_started = 1;
    snprintf(g_online_message, sizeof(g_online_message),
             "DOWNLOADING EXACT-MATCH NCL TO ARTEMIS USERLIST...");
}

static void service_community_download(void)
{
    if (!g_community_download_done) return;
    if (g_community_download_thread_started) {
        u64 thread_result = 0;
        (void)sysThreadJoin(g_community_download_thread, &thread_result);
        g_community_download_thread_started = 0;
    }
    if (g_community_download_result == 0) {
        const char *base = strrchr(g_community_download_path, '/');
        if (!g_community_download_persistent &&
            record_temporary_enhancement(g_community_download_path) != 0) {
            (void)unlink(g_community_download_path);
            snprintf(g_online_message, sizeof(g_online_message),
                     "TEMP CLEANUP COULD NOT BE RECORDED. PATCH FILE REMOVED.");
        } else {
            snprintf(g_online_message, sizeof(g_online_message),
                     "INSTALLED %.48s IN ARTEMIS USERLIST (%s). OPEN ARTEMIS TO SELECT IT.",
                     base ? base + 1 : g_community_download_path,
                     g_community_download_persistent ? "PERMANENT" : "TEMPORARY");
        }
    } else if (g_community_download_result == -2) {
        snprintf(g_online_message, sizeof(g_online_message),
                 "PATCH FILE ALREADY EXISTS. IT WAS NOT OVERWRITTEN.");
    } else if (g_community_download_result == -4) {
        snprintf(g_online_message, sizeof(g_online_message),
                 "DOWNLOAD REJECTED: FILE IS NOT SAFE NCL TEXT.");
    } else if (g_community_download_result == -5) {
        snprintf(g_online_message, sizeof(g_online_message),
                 "DOWNLOAD VALIDATED BUT COULD NOT BE SAVED.");
    } else if (g_community_download_result == -6) {
        snprintf(g_online_message, sizeof(g_online_message),
                 "SOURCE SIZE MISMATCH. REFRESH ONLINE DB AND RETRY.");
    } else if (g_community_download_result == -7) {
        snprintf(g_online_message, sizeof(g_online_message),
                 "GITHUB BLOB HASH MISMATCH. PATCH FILE REMOVED.");
    } else {
        snprintf(g_online_message, sizeof(g_online_message),
                 "COMMUNITY NCL DOWNLOAD FAILED. CHECK NETWORK AND SOURCE.");
    }
    g_community_download_expected_size = 0;
    g_community_download_expected_sha1[0] = '\0';
    g_community_download_done = 0;
    g_community_downloaded = 0;
    g_community_download_total = 0;
}

static void service_ps2_config_download(void){
 if(!g_ps2_config_download_done)return;
 if(g_ps2_config_download_thread_started){u64 thread_result=0;(void)sysThreadJoin(g_ps2_config_download_thread,&thread_result);g_ps2_config_download_thread_started=0;}
 if(g_ps2_config_download_result==0)set_details_message("PS2 CONFIG INSTALLED. GAME ISO AND EBOOT WERE NOT MODIFIED.");
 else if(g_ps2_config_download_result==-2)set_details_message("CONFIG CHANGED OR WAS NOT BACKED UP. NO REPLACEMENT WAS MADE.");
 else if(g_ps2_config_download_result==-4)set_details_message("DOWNLOADED CONFIG FAILED VALIDATION. FILE REMOVED.");
 else if(g_ps2_config_download_result==-5)set_details_message("COULD NOT INSTALL CONFIG. ORIGINAL FILE WAS PRESERVED.");
 else if(g_ps2_config_download_result==-6)set_details_message("ROLLBACK FILE ALREADY EXISTS. NO CONFIG WAS OVERWRITTEN.");
 else set_details_message("PS2 CONFIG DOWNLOAD FAILED. GAME FILES WERE NOT CHANGED.");
 snprintf(g_ps2_config_manager_message,sizeof(g_ps2_config_manager_message),"%s",g_details_message);
 g_ps2_config_download_done=0;g_ps2_config_downloaded=0;g_ps2_config_download_total=0;g_details_cache_valid=0;
 if(g_screen==SCREEN_PS2_CONFIG)refresh_ps2_config_manager_cache();
}
static int ps2_config_target_exists(const ps3scale_game_t *game,const ps3scale_ps2_config_entry_t *entry){
 char target[PS3SCALE_PATH_MAX],backup[PS3SCALE_PATH_MAX],bt[PS3SCALE_PATH_MAX],rollback[PS3SCALE_PATH_MAX],rt[PS3SCALE_PATH_MAX],dt[PS3SCALE_PATH_MAX];
 size_t max=PS3SCALE_PS2_CONFIG_MAX_BYTES;(void)entry;
 if(ps2_config_paths_for_game(game,target,sizeof(target),backup,sizeof(backup),bt,sizeof(bt),rollback,sizeof(rollback),rt,sizeof(rt),dt,sizeof(dt),&max)!=0)return 0;
 return ps3scale_ps2_config_store_probe(target,max,NULL)==1;
}

static void refresh_details_cache(void)
{
    const ps3scale_game_t *game = selected_game();
    const ps3scale_profile_t *profile = profile_for_game(game);
    const ps3scale_ps2_config_entry_t *ps2_config = ps2_config_for_game(game);

    /* Filesystem checks belong to screen entry / action completion, never
       to the render loop. Reopening these files every frame can stall pad input. */
    g_details_ps2_config_exists = ps2_config_target_exists(game, ps2_config);
    g_details_script_state = profile_script_state_for_game(game, profile);
    g_details_cache_valid = 1;
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
    g_artemis_installed =
        file_exists_readonly(PS3SCALE_ARTEMIS_EBOOT_PATH) ||
        file_exists_readonly(PS3SCALE_ARTEMIS_R5_EBOOT_PATH);
    g_webman_installed =
        file_exists_readonly("/dev_hdd0/plugins/webftp_server.sprx") ||
        file_contains_text_readonly("/dev_hdd0/boot_plugins.txt",
                                    "webftp_server.sprx");
    snprintf(g_tools_message, sizeof(g_tools_message),
             "STATUS UPDATED. SELECT APOLLO OR ARTEMIS.");
}

static void companion_tool_launch_worker(void *arg)
{
    (void)arg;
    g_tool_launch_result = ps3scale_webman_start_app(g_tool_launch_title_id);
    g_tool_launch_done = 1;
    g_tool_launch_running = 0;
    g_network_task_running = 0;
}

static void service_companion_tool_launch(void)
{
    if (!g_tool_launch_done)
        return;

    if (g_tool_launch_thread_started) {
        u64 thread_result = 0;
        (void)sysThreadJoin(g_tool_launch_thread, &thread_result);
        g_tool_launch_thread_started = 0;
    }
    g_tool_launch_done = 0;
    g_tool_launch_running = 0;

    if (g_tool_launch_result == 0) {
        snprintf(g_tools_message, sizeof(g_tools_message),
                 "WEBMAN ACCEPTED THE APP LAUNCH. RETURNING TO XMB.");
        g_exit_requested = 1;
    } else if (g_tool_launch_result == PS3SCALE_WEBMAN_ERROR_NETWORK_INIT) {
        snprintf(g_tools_message, sizeof(g_tools_message),
                 "PS3 NETWORK MODULES FAILED TO INITIALIZE.");
    } else if (g_tool_launch_result == PS3SCALE_WEBMAN_ERROR_SOCKET) {
        snprintf(g_tools_message, sizeof(g_tools_message),
                 "LOCAL WEBMAN SOCKET COULD NOT BE OPENED.");
    } else if (g_tool_launch_result == PS3SCALE_WEBMAN_ERROR_CONNECT) {
        snprintf(g_tools_message, sizeof(g_tools_message),
                 "WEBMAN MOD NOT RESPONDING ON LOCAL HTTP PORT 80.");
    } else if (g_tool_launch_result == PS3SCALE_WEBMAN_ERROR_SEND) {
        snprintf(g_tools_message, sizeof(g_tools_message),
                 "WEBMAN APP LAUNCH REQUEST COULD NOT BE SENT.");
    } else if (g_tool_launch_result == PS3SCALE_WEBMAN_ERROR_RESPONSE) {
        snprintf(g_tools_message, sizeof(g_tools_message),
                 "WEBMAN DID NOT RETURN AN HTTP RESPONSE.");
    } else if (g_tool_launch_result == PS3SCALE_WEBMAN_ERROR_HTTP_STATUS) {
        snprintf(g_tools_message, sizeof(g_tools_message),
                 "WEBMAN REJECTED THE COMPANION APP TITLE ID.");
    } else {
        snprintf(g_tools_message, sizeof(g_tools_message),
                 "INVALID COMPANION APP TITLE ID.");
    }
}

static void start_selected_companion_tool(void)
{
    const char *title_id = NULL;
    const char *tool_name = NULL;
    int rc;

    if (g_tools_selected == 0) {
        if (!g_apollo_installed) {
            snprintf(g_tools_message, sizeof(g_tools_message),
                     "APOLLO SAVE TOOL IS NOT INSTALLED.");
            return;
        }
        title_id = "NP0APOLLO";
        tool_name = "APOLLO SAVE TOOL";
    } else {
        if (file_exists_readonly(PS3SCALE_ARTEMIS_EBOOT_PATH)) {
            title_id = "ARTPS3001";
        } else if (file_exists_readonly(PS3SCALE_ARTEMIS_R5_EBOOT_PATH)) {
            title_id = "ARTZ00001";
        } else {
            snprintf(g_tools_message, sizeof(g_tools_message),
                     "ARTEMIS PS3 IS NOT INSTALLED.");
            return;
        }
        tool_name = "ARTEMIS PS3";
    }

    /*
     * The plugin path check is only a hint. Some CFW setups load webMAN from
     * a non-standard path; the bounded local HTTP request below is the
     * authoritative test. Do not block launch solely on a missing file hint.
     */
    if (g_network_task_running || g_tool_launch_running ||
        g_tool_launch_done || g_tool_launch_thread_started) {
        snprintf(g_tools_message, sizeof(g_tools_message),
                 "WAIT FOR THE CURRENT NETWORK TASK TO FINISH.");
        return;
    }

    snprintf(g_tool_launch_title_id, sizeof(g_tool_launch_title_id),
             "%s", title_id);
    g_tool_launch_result = -1;
    g_tool_launch_done = 0;
    g_tool_launch_running = 1;
    g_network_task_running = 1;
    rc = sysThreadCreate(&g_tool_launch_thread,
                         companion_tool_launch_worker, NULL,
                         1500, 0x8000, THREAD_JOINABLE,
                         "PS3 SCALE Tool Launch");
    if (rc == 0) {
        g_tool_launch_thread_started = 1;
        if (g_webman_installed) {
            snprintf(g_tools_message, sizeof(g_tools_message),
                     "OPENING %s THROUGH WEBMAN MOD...", tool_name);
        } else {
            snprintf(g_tools_message, sizeof(g_tools_message),
                     "WEBMAN FILE HINT MISSING; TRYING LOCAL SERVICE...");
        }
    } else {
        g_tool_launch_running = 0;
        g_network_task_running = 0;
        snprintf(g_tools_message, sizeof(g_tools_message),
                 "COULD NOT START THE COMPANION TOOL WORKER.");
    }
}

static void draw_rounded_rect(float x, float y, float w, float h,
                              float radius, u32 color);
static void draw_rounded_outline(float x, float y, float w, float h,
                                 float radius, u32 color);

static void draw_language_selection(void)
{
    int i;

    draw_rounded_rect(92.0f, 72.0f, 670.0f, 390.0f, 18.0f, 0xfaf8fff0);
    draw_rounded_outline(92.0f, 72.0f, 670.0f, 390.0f, 18.0f, 0xd9d1eaff);
    draw_ps_symbol(128.0f, 96.0f, 0x29243fff);

    set_ui_font_size(23, 35);
    draw_text(180.0f, 96.0f, 0x29243fff, "PS3 SCALE");
    set_ui_font_size(10, 15);
    draw_text(128.0f, 150.0f, 0x514b69ff, "CHOOSE LANGUAGE");
    draw_text(128.0f, 178.0f, 0x514b69ff, "اختر اللغة");
    draw_text(128.0f, 204.0f, 0x514b69ff, "Elige el idioma");

    for (i = 0; i < 3; i++) {
        float y = 250.0f + (float)i * 50.0f;
        int selected = (i == g_language_choice);

        draw_rounded_rect(128.0f, y, 598.0f, 40.0f, 9.0f,
                          selected ? 0xeee7ffff : 0xf8f6ffff);
        draw_rounded_outline(128.0f, y, 598.0f, 40.0f, 9.0f,
                             selected ? 0x9568edff : 0xd9d1eaff);
        if (selected)
            draw_rounded_rect(138.0f, y + 8.0f, 3.0f, 24.0f, 1.5f,
                              0x875be8ff);

        set_ui_font_size(9, 14);
        draw_text(158.0f, y + 12.0f,
                  selected ? 0x875be8ff : 0x514b69ff,
                  ps3scale_locale_language_name(i));
        if (selected) {
            draw_circle_outline(694.0f, y + 20.0f, 7.0f, 0x875be8ff);
            draw_circle_outline(694.0f, y + 20.0f, 3.0f, 0x875be8ff);
        }
    }

    set_ui_font_size(7, 11);
    draw_text(128.0f, 424.0f, 0x77718fff,
              "UP / DOWN: SELECT   |   CROSS: CONTINUE");
    set_ui_font_size(14, 22);
}

static void draw_splash(void)
{
    unsigned alpha = 255u;
    int frame = g_splash_frames;
    float progress = !g_animation_enabled ? 1.0f :
                     (frame >= 179 ? 1.0f : (float)frame / 179.0f);
    char progress_text[12];

    if (!g_animation_enabled) {
        alpha = 255u;
    } else if (frame < 30) {
        alpha = (unsigned)(frame * 255 / 30);
    }
    if (alpha > 255u)
        alpha = 255u;

    /* Pearl glass hero panel over the animated staircase scene. */
    draw_rounded_rect(104.0f, 54.0f, 646.0f, 410.0f, 18.0f,
                      color_with_alpha(0x8b73c8ff, 26u * alpha / 255u));
    draw_rounded_rect(98.0f, 48.0f, 646.0f, 410.0f, 18.0f,
                      color_with_alpha(0xfaf8ffff, 232u * alpha / 255u));
    draw_rounded_outline(98.0f, 48.0f, 646.0f, 410.0f, 18.0f,
                         color_with_alpha(0xd9d1eaff, alpha));

    set_ui_font_size(7, 11);
    draw_text(128.0f, 70.0f,
              color_with_alpha(0x77718fff, alpha),
              "PS3 SCALE / CONSOLE MANAGEMENT");
    draw_text(654.0f, 70.0f,
              color_with_alpha(0x875be8ff, alpha), PS3SCALE_APP_VERSION);

    draw_ps_symbol(405.0f, 91.0f,
                   color_with_alpha(0x29243fff, alpha));
    set_ui_font_size(32, 48);
    draw_text(298.0f, 132.0f,
              color_with_alpha(0x29243fff, alpha), "PS3");
    draw_text(399.0f, 132.0f,
              color_with_alpha(0x875be8ff, alpha), "SCALE");

    set_ui_font_size(10, 15);
    draw_text(267.0f, 190.0f,
              color_with_alpha(0x514b69ff, alpha),
              "PS3 & PS2 Enhancement Management");

    draw_solid_rect(164.0f, 224.0f, 520.0f, 1.5f,
                    color_with_alpha(0xd9d1eaff, alpha));
    set_ui_font_size(9, 14);
    draw_text(360.0f, 240.0f,
              color_with_alpha(0x514b69ff, alpha), "SPECIAL THANKS");

    /* Artemis and Apollo remain separate partner cards and keep their marks. */
    draw_rounded_rect(210.0f, 270.0f, 196.0f, 92.0f, 12.0f,
                      color_with_alpha(0xf3efffff, alpha));
    draw_rounded_outline(210.0f, 270.0f, 196.0f, 92.0f, 12.0f,
                         color_with_alpha(0xd9d1eaff, alpha));
    draw_circle_outline(308.0f, 300.0f, 19.0f,
                        color_with_alpha(0x875be8ff, alpha));
    draw_icon_line(293.0f, 317.0f, 308.0f, 282.0f,
                   color_with_alpha(0x875be8ff, alpha));
    draw_icon_line(308.0f, 282.0f, 323.0f, 317.0f,
                   color_with_alpha(0x875be8ff, alpha));
    draw_icon_line(298.0f, 305.0f, 318.0f, 305.0f,
                   color_with_alpha(0x875be8ff, alpha));
    set_ui_font_size(8, 12);
    draw_text(274.0f, 332.0f,
              color_with_alpha(0x29243fff, alpha), "ARTEMIS");

    draw_rounded_rect(448.0f, 270.0f, 196.0f, 92.0f, 12.0f,
                      color_with_alpha(0xf3efffff, alpha));
    draw_rounded_outline(448.0f, 270.0f, 196.0f, 92.0f, 12.0f,
                         color_with_alpha(0xd9d1eaff, alpha));
    if (g_apollo_texture) {
        ya2d_drawTextureEx(g_apollo_texture, 466.0f, 284.0f,
                           0.0f, 64.0f, 64.0f);
        draw_solid_rect(466.0f, 284.0f, 64.0f, 64.0f,
                        color_with_alpha(0xf3efffff, 255u - alpha));
    } else {
        draw_circle_outline(498.0f, 316.0f, 22.0f,
                            color_with_alpha(0xb995ffff, alpha));
        draw_icon_line(484.0f, 316.0f, 512.0f, 316.0f,
                       color_with_alpha(0xb995ffff, alpha));
    }
    set_ui_font_size(7, 11);
    draw_text(538.0f, 306.0f,
              color_with_alpha(0x29243fff, alpha), "APOLLO SAVE");

    set_ui_font_size(7, 11);
    draw_text(210.0f, 382.0f,
              color_with_alpha(0x514b69ff, alpha), "LOADING PS3 SCALE");
    snprintf(progress_text, sizeof(progress_text), "%u%%",
             (unsigned)(progress * 100.0f));
    draw_text(616.0f, 382.0f,
              color_with_alpha(0x875be8ff, alpha), progress_text);

    draw_rounded_rect(210.0f, 402.0f, 434.0f, 11.0f, 5.5f,
                      color_with_alpha(0xe2dcf3ff, alpha));
    draw_rounded_outline(210.0f, 402.0f, 434.0f, 11.0f, 5.5f,
                         color_with_alpha(0xd4c9edff, alpha));
    if (progress > 0.0f)
        draw_rounded_rect(213.0f, 405.0f, 428.0f * progress, 5.0f, 2.5f,
                          color_with_alpha(0x875be8ff, alpha));

    set_ui_font_size(7, 11);
    draw_text(312.0f, 432.0f,
              color_with_alpha(0x77718fff, alpha),
              "PRESS CROSS OR START TO SKIP");
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
    float moving = 136.0f + (float)((g_frame_counter * 5u) % 500u);
    u32 status_color = g_app_files_ok ? 0x2c9f8eff : 0xc58a4dff;

    draw_rounded_rect(92.0f, 118.0f, 670.0f, 260.0f, 16.0f, 0xfaf8ffff);
    draw_rounded_outline(92.0f, 118.0f, 670.0f, 260.0f, 16.0f, 0xd9d1eaff);

    draw_ps_symbol(132.0f, 148.0f, 0x29243fff);
    set_ui_font_size(25, 37);
    draw_text(184.0f, 142.0f, 0x29243fff, "PS3");
    draw_text(258.0f, 142.0f, 0x875be8ff, "SCALE");
    set_ui_font_size(8, 12);
    draw_text(132.0f, 204.0f, 0x514b69ff,
              "Starting your game library");

    draw_rounded_rect(132.0f, 270.0f, 590.0f, 16.0f, 8.0f, 0xe2dcf3ff);
    draw_rounded_outline(132.0f, 270.0f, 590.0f, 16.0f, 8.0f, 0xd4c9edff);
    draw_rounded_rect(136.0f, 274.0f, width, 8.0f, 4.0f, 0x875be8ff);
    if (g_boot_stage == 2)
        draw_rounded_rect(moving, 274.0f, 62.0f, 8.0f, 4.0f, 0xb995ffff);

    set_ui_font_size(8, 12);
    draw_text(132.0f, 316.0f, 0x514b69ff, stage_text[g_boot_stage]);

    draw_rounded_rect(132.0f, 344.0f, 280.0f, 24.0f, 8.0f,
                      g_app_files_ok ? 0xe4f5ecff : 0xfff2e2ff);
    draw_rounded_outline(132.0f, 344.0f, 280.0f, 24.0f, 8.0f,
                         status_color);
    set_ui_font_size(7, 11);
    draw_text(146.0f, 350.0f, status_color,
              g_app_files_ok ? "APP FILE CHECK: OK" :
                               "APP FILE CHECK: VERIFY INSTALL");
    set_ui_font_size(14, 22);
}

static void copy_text_tail_preview(char *destination, size_t capacity,
                                     const char *source)
{
    size_t length;
    size_t start;

    if (!destination || capacity == 0)
        return;
    destination[0] = '\0';
    if (!source)
        return;
    length = strlen(source);
    if (length < capacity) {
        memcpy(destination, source, length + 1);
        return;
    }
    start = length - (capacity - 4);
    while (start < length &&
           (((unsigned char)source[start] & 0xc0u) == 0x80u))
        start++;
    destination[0] = '.';
    destination[1] = '.';
    destination[2] = '.';
    snprintf(destination + 3, capacity - 3, "%s", source + start);
}

static void draw_keyboard(void)
{
    int i;
    char search_preview[128];
    copy_text_tail_preview(search_preview, sizeof(search_preview), g_search);
    const char *title = g_keyboard_support_mode ?
        (g_support_selected == 0 ? "REPORT TITLE" : "REPORT DETAILS") :
        g_keyboard_online_mode ? "SEARCH ONLINE DATABASE" : "SEARCH GAMES";
    const char *subtitle = g_keyboard_support_mode ?
        "Enter text for your support report" :
        g_keyboard_online_mode ? "Type to search the online database" :
        "Type to filter the selected game list";

    draw_ps_symbol(22.0f, 11.0f, 0x29243fff);
    set_ui_font_size(15, 23);
    draw_text(68.0f, 16.0f, 0x29243fff, "PS3 SCALE");
    set_ui_font_size(10, 15);
    draw_text(218.0f, 22.0f, 0x875be8ff, title);
    set_ui_font_size(7, 11);
    draw_text(26.0f, 49.0f, 0x77718fff, subtitle);

    draw_rounded_rect(40.0f, 112.0f, 768.0f, 34.0f, 8.0f, 0xfaf8ffff);
    draw_rounded_outline(40.0f, 112.0f, 768.0f, 34.0f,
                         8.0f, 0xbba5f0ff);
    draw_text(54.0f, 120.0f, 0x29243fff,
              search_preview[0] ? search_preview : " ");

    for (i = 0; i < 40; i++) {
        int row = i / 10;
        int col = i % 10;
        float x = 48.0f + (float)col * 76.0f;
        float y = 160.0f + (float)row * 46.0f;
        int selected = (i == g_keyboard_selected);

        draw_rounded_rect(x, y, 68.0f, 38.0f, 7.0f,
                          selected ? 0xeee7ffff : 0xfaf8ffff);
        draw_rounded_outline(x, y, 68.0f, 38.0f, 7.0f,
                             selected ? 0x9568edff : 0xd9d1eaff);
        if (selected)
            draw_rounded_rect(x + 6.0f, y + 5.0f, 3.0f, 28.0f,
                              1.5f, 0x875be8ff);
        set_ui_font_size(8, 12);
        draw_text(x + 23.0f, y + 9.0f,
                  selected ? 0x875be8ff : 0x29243fff,
                  ps3scale_locale_keyboard_key(g_language, i));
    }

    {
        static const char *actions[5] = {
            "SPACE", "BACKSPACE", "CLEAR", "APPLY", "CANCEL"
        };
        for (i = 0; i < 5; i++) {
            float x = 36.0f + (float)i * 156.0f;
            int selected = (g_keyboard_selected == 40 + i);

            draw_rounded_rect(x, 360.0f, 144.0f, 42.0f, 8.0f,
                              selected ? 0xeee7ffff : 0xfaf8ffff);
            draw_rounded_outline(x, 360.0f, 144.0f, 42.0f, 8.0f,
                                 selected ? 0x9568edff : 0xd9d1eaff);
            set_ui_font_size(7, 11);
            draw_text(x + 14.0f, 374.0f,
                      selected ? 0x875be8ff : 0x514b69ff, actions[i]);
        }
    }

    set_ui_font_size(7, 11);
    draw_text(34.0f, 440.0f, 0x77718fff,
              "ARROWS MOVE  |  CROSS SELECT  |  CIRCLE CANCEL");
    draw_text(34.0f, PS3SCALE_UI_FOOTER_SINGLE_Y, 0x875be8ff,
              "CUSTOM KEYBOARD  |  NO PS3 SYSTEM KEYBOARD REQUIRED");
    set_ui_font_size(14, 22);
}


/* Light-weight rounded panels: rendered as two rectangular strips plus a
   single batch of corner triangles, with no textures or frame allocations. */
static void draw_rounded_rect(float x, float y, float w, float h,
                              float radius, u32 color)
{
    int corner, segment, colored = 0;
    const int segments = 6;
    const float pi = 3.14159265359f;
    float r = radius;
    float cx[4], cy[4], start[4];

    if (r > w * 0.5f) r = w * 0.5f;
    if (r > h * 0.5f) r = h * 0.5f;
    if (r <= 0.0f) {
        draw_solid_rect(x, y, w, h, color);
        return;
    }

    draw_solid_rect(x + r, y, w - r * 2.0f, h, color);
    draw_solid_rect(x, y + r, r, h - r * 2.0f, color);
    draw_solid_rect(x + w - r, y + r, r, h - r * 2.0f, color);

    cx[0] = x + w - r; cy[0] = y + r;     start[0] = pi * 1.5f;
    cx[1] = x + w - r; cy[1] = y + h - r; start[1] = 0.0f;
    cx[2] = x + r;     cy[2] = y + h - r; start[2] = pi * 0.5f;
    cx[3] = x + r;     cy[3] = y + r;     start[3] = pi;

    tiny3d_SetPolygon(TINY3D_TRIANGLES);
    for (corner = 0; corner < 4; corner++) {
        for (segment = 0; segment < segments; segment++) {
            float a = start[corner] + (pi * 0.5f) *
                      (float)segment / (float)segments;
            float b = start[corner] + (pi * 0.5f) *
                      (float)(segment + 1) / (float)segments;
            tiny3d_VertexPos(cx[corner], cy[corner], 65535.0f);
            if (!colored) {
                tiny3d_VertexColor(ui_theme_color(color));
                colored = 1;
            }
            tiny3d_VertexPos(cx[corner] + cosf(a) * r,
                             cy[corner] + sinf(a) * r, 65535.0f);
            tiny3d_VertexPos(cx[corner] + cosf(b) * r,
                             cy[corner] + sinf(b) * r, 65535.0f);
        }
    }
    tiny3d_End();
}

static void draw_rounded_outline(float x, float y, float w, float h,
                                 float radius, u32 color)
{
    int corner, segment;
    const int segments = 6;
    const float pi = 3.14159265359f;
    float r = radius;
    float cx[4], cy[4], start[4];

    if (r > w * 0.5f) r = w * 0.5f;
    if (r > h * 0.5f) r = h * 0.5f;

    cx[0] = x + w - r; cy[0] = y + r;     start[0] = pi * 1.5f;
    cx[1] = x + w - r; cy[1] = y + h - r; start[1] = 0.0f;
    cx[2] = x + r;     cy[2] = y + h - r; start[2] = pi * 0.5f;
    cx[3] = x + r;     cy[3] = y + r;     start[3] = pi;

    tiny3d_SetPolygon(TINY3D_LINE_LOOP);
    for (corner = 0; corner < 4; corner++) {
        for (segment = 0; segment <= segments; segment++) {
            float angle = start[corner] +
                pi * 0.5f * (float)segment / (float)segments;
            tiny3d_VertexPos(cx[corner] + cosf(angle) * r,
                             cy[corner] + sinf(angle) * r, 65535.0f);
            if (corner == 0 && segment == 0)
                tiny3d_VertexColor(ui_theme_color(color));
        }
    }
    tiny3d_End();
}

static void draw_soft_card(float x, float y, float w, float h, int selected)
{
    const float corner = 9.0f;
    draw_rounded_rect(x + 3.0f, y + 6.0f, w, h, corner,
                      selected ? 0x665b8f50 : 0x5e718f35);
    draw_rounded_rect(x, y, w, h, corner,
                      selected ? 0xf4efffe8 : 0xfaf8ffe8);
    draw_rounded_outline(x, y, w, h, corner,
                         selected ? 0x9568edff : 0xd9d1eaff);
    draw_solid_rect(x + 10.0f, y + 5.0f, w - 20.0f, 1.5f, 0xfefcffb8);
    if (selected) {
        draw_rounded_outline(x - 2.0f, y - 2.0f,
                             w + 4.0f, h + 4.0f, corner + 2.0f,
                             0xb99aff70);
        draw_solid_rect(x + 12.0f, y + h - 5.0f,
                        w - 24.0f, 2.5f, 0x875be8ff);
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
    draw_rounded_rect(x, y, w, 24.0f, 7.0f,
                    selected ? 0xeee7ffff : 0xf8f6ffff);
    draw_rounded_outline(x, y, w, 24.0f, 7.0f,
                      selected ? 0x9568edff : 0xd9d1eaff);
    set_ui_font_size(7, 11);
    draw_text_centered(x + 4.0f, y + 7.0f, w - 8.0f,
                       selected ? 0x875be8ff : 0x514b69ff, label);
}

static void draw_page_header(const char *title, const char *subtitle)
{
    draw_ps_symbol(22.0f, 11.0f, 0x29243fff);
    set_ui_font_size(15, 23);
    draw_text(68.0f, 16.0f, 0x29243fff, "PS3 SCALE");
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
                       selected ? 0x875be8ff : 0x29243fff, label);
}

static void draw_home_animated_card(float x, float y, float w, float h,
                                     float focus)
{
    float scale, card_w, card_h, card_x, card_y;
    float pulse;
    unsigned int halo_alpha, ring_alpha;

    if (focus < 0.0f) focus = 0.0f;
    if (focus > 1.0f) focus = 1.0f;

    scale = 1.0f + focus * 0.035f;
    card_w = w * scale;
    card_h = h * scale;
    card_x = x - (card_w - w) * 0.5f;
    card_y = y - (card_h - h) * 0.5f - focus * 5.0f;

    draw_rounded_rect(card_x + 3.0f, card_y + 6.0f,
                      card_w, card_h, 9.0f,
                      ui_blend_color(0x5e718f35, 0x665b8f50, focus));
    draw_rounded_rect(card_x, card_y, card_w, card_h, 9.0f,
                      ui_blend_color(0xfaf8ffe8, 0xf4efffe8, focus));
    draw_rounded_outline(card_x, card_y, card_w, card_h, 9.0f,
                         ui_blend_color(0xd9d1eaff, 0x9568edff, focus));
    draw_solid_rect(card_x + 10.0f, card_y + 5.0f,
                    card_w - 20.0f, 1.5f, 0xfefcffb8);

    if (focus > 0.01f) {
        halo_alpha = (unsigned int)(focus * 64.0f);
        draw_rounded_outline(card_x - 2.0f, card_y - 2.0f,
                             card_w + 4.0f, card_h + 4.0f,
                             11.0f,
                             color_with_alpha(0xb99affff, halo_alpha));
        draw_rounded_rect(card_x + 12.0f, card_y + card_h - 5.0f,
                          card_w - 24.0f, 2.5f, 1.25f,
                          color_with_alpha(0x875be8ff,
                                           (unsigned int)(focus * 255.0f)));
    }

    if (focus > 0.04f) {
        pulse = g_animation_enabled ?
                0.5f + 0.5f * sinf((float)g_frame_counter * 0.12f) :
                0.5f;
        ring_alpha = (unsigned int)(focus * (48.0f + 48.0f * pulse));
        draw_circle_outline(card_x + card_w * 0.5f,
                            card_y + card_h - 18.0f,
                            4.0f + 1.5f * pulse,
                            color_with_alpha(0x9568edff, ring_alpha));
        draw_circle_outline(card_x + card_w * 0.5f,
                            card_y + card_h - 18.0f,
                            7.0f + 2.0f * pulse,
                            color_with_alpha(0xb99affff, ring_alpha / 2u));
    }
}

static void update_home_card_animations(void)
{
    int i;

    for (i = 0; i < 5; i++) {
        float target = (i == g_home_selected) ? 1.0f : 0.0f;
        float difference = target - g_home_card_focus[i];

        if (!g_animation_enabled) {
            g_home_card_focus[i] = target;
        } else {
            g_home_card_focus[i] += difference * 0.24f;
            if (fabsf(difference) < 0.006f)
                g_home_card_focus[i] = target;
        }
    }
}

static void update_scan_card_animations(void)
{
    size_t count = library_count(g_library);
    size_t start = ((size_t)(g_selected < 0 ? 0 : g_selected) /
                    PS3SCALE_SCAN_GRID_COUNT) * PS3SCALE_SCAN_GRID_COUNT;
    size_t i;

    for (i = 0; i < PS3SCALE_SCAN_GRID_COUNT; i++) {
        float target = (g_screen == SCREEN_SCAN &&
                        start + i < count &&
                        (int)(start + i) == g_selected) ? 1.0f : 0.0f;
        float difference = target - g_scan_card_focus[i];

        if (!g_animation_enabled) {
            g_scan_card_focus[i] = target;
        } else {
            g_scan_card_focus[i] += difference * 0.28f;
            if (fabsf(difference) < 0.006f)
                g_scan_card_focus[i] = target;
        }
    }
}

static void draw_library_animated_card(float x, float y, float w, float h,
                                       float focus)
{
    float pulse;
    unsigned int halo_alpha;

    if (focus < 0.0f) focus = 0.0f;
    if (focus > 1.0f) focus = 1.0f;

    draw_rounded_rect(x + 3.0f, y + 6.0f, w, h, 9.0f,
                      ui_blend_color(0x5e718f35, 0x665b8f50, focus));
    draw_rounded_rect(x, y, w, h, 9.0f,
                      ui_blend_color(0xfaf8ffe8, 0xf4efffe8, focus));
    draw_rounded_outline(x, y, w, h, 9.0f,
                         ui_blend_color(0xd9d1eaff, 0x9568edff, focus));
    draw_solid_rect(x + 10.0f, y + 5.0f, w - 20.0f, 1.5f,
                    0xfefcffb8);

    if (focus > 0.01f) {
        halo_alpha = (unsigned int)(focus * 72.0f);
        draw_rounded_outline(x - 2.0f, y - 2.0f, w + 4.0f, h + 4.0f,
                             11.0f,
                             color_with_alpha(0xb99affff, halo_alpha));
        draw_rounded_rect(x + 12.0f, y + h - 5.0f, w - 24.0f, 2.5f,
                          1.25f,
                          color_with_alpha(0x875be8ff,
                                           (unsigned int)(focus * 255.0f)));
        pulse = g_animation_enabled ?
                0.5f + 0.5f * sinf((float)g_frame_counter * 0.12f) : 0.5f;
        draw_circle_outline(x + w * 0.5f, y + h - 17.0f,
                            3.0f + pulse * 1.5f,
                            color_with_alpha(0x9568edff,
                                             (unsigned int)(focus * 72.0f)));
    }
}

static void update_online_row_animation(void)
{
    float target = (g_screen == SCREEN_ONLINE_DB &&
                    g_online_result_count > 0) ? 1.0f : 0.0f;

    if (target > 0.0f && g_online_focus_selection != g_online_selected) {
        g_online_focus_selection = g_online_selected;
        g_online_row_focus = 0.0f;
    }

    if (!g_animation_enabled) {
        g_online_row_focus = target;
    } else {
        float difference = target - g_online_row_focus;
        g_online_row_focus += difference * 0.28f;
        if (fabsf(difference) < 0.006f)
            g_online_row_focus = target;
    }
}

static void draw_home(void)
{
    static const char *items[5] = {
        "Scan Games", "Online DB", "Settings", "Help", "About"
    };
    static const char *descriptions[5] = {
        "PS3 / PS2 Library", "Game Profiles", "Music & Theme",
        "Guide & Support", "App Info"
    };
    char line[192];
    char firmware_label[24];
    char model_label[32];
    int i;

    if (g_hardware_info_valid) {
        snprintf(firmware_label, sizeof(firmware_label), "FW %s",
                 g_hardware_info.firmware_version);
        snprintf(model_label, sizeof(model_label), "PS3 %s",
                 ps3scale_hardware_family_name(g_hardware_info.family));
    } else {
        snprintf(firmware_label, sizeof(firmware_label), "FW UNKNOWN");
        snprintf(model_label, sizeof(model_label), "PS3 UNKNOWN");
    }

    draw_ps_symbol(25.0f, 17.0f, 0x29243fff);
    set_ui_font_size(25, 37);
    draw_text(70.0f, 18.0f, 0x29243fff, "PS3");
    draw_text(148.0f, 18.0f, 0x875be8ff, "SCALE");
    set_ui_font_size(8, 12);
    draw_text(72.0f, 61.0f, 0x514b69ff,
              "PS3 & PS2 ENHANCEMENT MANAGEMENT");
    draw_solid_rect(72.0f, 82.0f, 252.0f, 2.0f, 0xb69affff);
    draw_text(72.0f, 98.0f, 0x77718fff,
              "EXPLORE  |  ENHANCE  |  PLAY");

    draw_soft_card(568.0f, 18.0f, 116.0f, 32.0f, 0);
    draw_soft_card(692.0f, 18.0f, 148.0f, 32.0f, 0);
    set_ui_font_size(7, 11);
    draw_text(582.0f, 28.0f, 0x875be8ff, firmware_label);
    draw_text(705.0f, 28.0f, 0x514b69ff, model_label);
    set_ui_font_size(14, 22);

    for (i = 0; i < 5; i++) {
        float x = 96.0f + (float)i * 134.0f;
        float y = 148.0f;
        const float w = 126.0f, h = 150.0f;
        float focus = g_home_card_focus[i];
        float scale = 1.0f + focus * 0.035f;
        float card_w = w * scale;
        float card_h = h * scale;
        float card_x = x - (card_w - w) * 0.5f;
        float card_y = y - (card_h - h) * 0.5f - focus * 5.0f;
        int selected = (focus >= 0.5f);

        draw_home_animated_card(x, y, w, h, focus);
        draw_menu_icon(i == 4 ? 5 : i,
                       card_x + (card_w - 42.0f) * 0.5f,
                       card_y + 16.0f, selected);
        set_ui_font_size(8, 12);
        draw_text_centered(card_x + 5.0f, card_y + 77.0f,
                           card_w - 10.0f,
                           ui_blend_color(0x29243fff, 0x875be8ff, focus),
                           items[i]);
        set_ui_font_size(6, 9);
        draw_text_centered(card_x + 6.0f, card_y + 103.0f,
                           card_w - 12.0f, 0x77718fff, descriptions[i]);
    }

    snprintf(line, sizeof(line), "PS3  %u     PS2  %u     TOTAL  %u",
             (unsigned)library_total_count(LIBRARY_PS3),
             (unsigned)library_total_count(LIBRARY_PS2),
             (unsigned)(library_total_count(LIBRARY_PS3) +
                        library_total_count(LIBRARY_PS2)));
    draw_soft_card(30.0f, 392.0f, 322.0f, 42.0f, 0);
    set_ui_font_size(8, 12);
    draw_text(46.0f, 407.0f, 0x514b69ff, line);
    draw_text(31.0f, PS3SCALE_UI_FOOTER_PRIMARY_Y, 0x77718fff,
              "D-PAD LEFT / RIGHT: MOVE   |   CROSS: SELECT   |   CIRCLE: EXIT");
    draw_text(31.0f, PS3SCALE_UI_FOOTER_SECONDARY_Y, 0x875be8ff,
              "SELECT: COMPANION TOOLS");
    set_ui_font_size(14, 22);
}

static void draw_online_database(void)
{
    const ps3scale_profile_t *selected_profile = NULL;
    const ps3scale_community_file_t *selected_community = NULL;
    const ps3scale_ps2_config_entry_t *selected_ps2_config = NULL;
    const ps3scale_game_t *selected = NULL;
    const size_t page_size = PS3SCALE_ONLINE_PAGE_SIZE;
    size_t page_start = 0;
    size_t total_pages = 0;
    size_t current_page = 0;
    size_t i, visible_count = 0;
    char line[192];

    if (g_online_result_count) {
        page_start = ((size_t)g_online_selected / page_size) * page_size;
        total_pages = (g_online_result_count + page_size - 1u) / page_size;
        current_page = page_start / page_size + 1u;
    }

    draw_page_header("Online DB", "ONLINE GAME DATABASE | PS3 profiles, GitHub data and PS2 configs");
    draw_soft_card(20.0f, 88.0f, 360.0f, 360.0f, 0);
    draw_soft_card(394.0f, 88.0f, 440.0f, 360.0f, 0);

    set_ui_font_size(10, 15);
    draw_text(38.0f, 102.0f, 0x29243fff, "Search & Browse");
    set_ui_font_size(7, 11);
    snprintf(line, sizeof(line), "QUERY: %.36s",
             g_online_query[0] ? g_online_query : "Installed games");
    draw_text(38.0f, 125.0f, 0x514b69ff, line);
    set_ui_font_size(6, 9);
    snprintf(line, sizeof(line), "FILTER: %.42s", online_filter_label());
    draw_text(38.0f, 149.0f, 0x875be8ff, line);

    selected = online_result_at((size_t)g_online_selected,
                                &selected_profile,
                                &selected_community,
                                &selected_ps2_config);

    /* Only draw the selected result's page. This fixes the old first-eight
       behavior where navigating beyond result #8 hid the selection. */
    for (i = page_start;
         i < g_online_result_count && visible_count < page_size;
         i++) {
        const ps3scale_profile_t *profile = NULL;
        const ps3scale_community_file_t *community = NULL;
        const ps3scale_ps2_config_entry_t *ps2_config = NULL;
        const ps3scale_game_t *game =
            online_result_at(i, &profile, &community, &ps2_config);
        const char *title, *kind;
        u32 badge_color;
        int focused = ((int)i == g_online_selected);
        float focus = focused ? g_online_row_focus : 0.0f;
        float scale = 1.0f + focus * 0.02f;
        float row_w = 334.0f * scale;
        float row_h = 27.0f * scale;
        float y = 178.0f + (float)visible_count * 30.0f;
        float row_x = 32.0f - (row_w - 334.0f) * 0.5f;
        float row_y = y - (row_h - 27.0f) * 0.5f - focus * 1.0f;
        char row[128];

        if (!game && !community && !ps2_config)
            continue;

        if (ps2_config) {
            title = ps2_config->title;
            kind = "PS2 CFG";
            badge_color = 0x7e67baff;
        } else if (community) {
            title = community->title[0] ? community->title :
                    (game ? game->title : "GitHub data file");
            if (game && ps3scale_community_file_matches_exact_game(
                            community, game)) {
                kind = "VER OK";
                badge_color = 0x468f78ff;
            } else if (game) {
                kind = "REVIEW";
                badge_color = 0xae884bff;
            } else {
                kind = "GITHUB";
                badge_color = 0x7e67baff;
            }
        } else {
            title = game ? game->title : "Game profile";
            kind = "PROFILE";
            badge_color = 0x468f78ff;
        }

        draw_rounded_rect(row_x + 2.0f, row_y + 3.0f,
                          row_w, row_h, 7.0f * scale,
                          ui_blend_color(0x5e718f25, 0x665b8f35, focus));
        draw_rounded_rect(row_x, row_y, row_w, row_h, 7.0f * scale,
                          ui_blend_color(0xfaf9ffed, 0xf2eafff4, focus));
        draw_rounded_outline(row_x, row_y, row_w, row_h, 7.0f * scale,
                             ui_blend_color(0xe6dff1ff, 0x9568edff, focus));
        if (focus > 0.01f)
            draw_rounded_rect(row_x + 4.0f * scale,
                              row_y + 5.0f * scale,
                              2.5f * scale, row_h - 10.0f * scale,
                              1.2f * scale,
                              color_with_alpha(0x875be8ff,
                                               (unsigned int)(focus * 255.0f)));
        draw_rounded_rect(row_x + 10.0f * scale, row_y + 5.0f * scale,
                          20.0f * scale, 18.0f * scale, 5.0f * scale,
                          focused ? 0x875be8ff : 0xe9e2f6ff);
        draw_solid_rect(row_x + 15.0f * scale, row_y + 9.0f * scale,
                        10.0f * scale, 2.0f * scale,
                        focused ? 0xfaf7ffff : 0xa58acbff);
        draw_solid_rect(row_x + 15.0f * scale, row_y + 14.0f * scale,
                        7.0f * scale, 2.0f * scale,
                        focused ? 0xfaf7ffff : 0xa58acbff);

        set_ui_font_size(6, 9);
        snprintf(row, sizeof(row), "%.28s", title);
        draw_text(row_x + 38.0f * scale, row_y + 9.0f * scale,
                  0x29243fff, row);
        draw_text(row_x + 266.0f * scale, row_y + 9.0f * scale,
                  focused ? 0x875be8ff : badge_color, kind);
        visible_count++;
    }

    set_ui_font_size(7, 11);
    if (g_online_result_count) {
        snprintf(line, sizeof(line), "RESULTS %u  |  PAGE %u/%u",
                 (unsigned)g_online_result_count,
                 (unsigned)current_page, (unsigned)total_pages);
    } else {
        snprintf(line, sizeof(line), "RESULTS 0  |  NO MATCHES");
    }
    draw_text(38.0f, 414.0f, 0x514b69ff, line);

    /* Cache status is not a claim that GitHub is currently online or that
       any community file is safe to auto-apply. */
    set_ui_font_size(10, 15);
    draw_text(414.0f, 102.0f, 0x29243fff, "Selected Result");
    set_ui_font_size(7, 11);
    if (g_catalog_sync_running) {
        unsigned long downloaded_kb =
            (unsigned long)((g_catalog_sync_downloaded + 1023u) / 1024u);
        unsigned long total_kb =
            (unsigned long)((g_catalog_sync_total + 1023u) / 1024u);
        if (g_catalog_sync_total > 0) {
            unsigned int percent = (unsigned int)(
                (g_catalog_sync_downloaded >= g_catalog_sync_total) ? 100u :
                (g_catalog_sync_downloaded * 100u) / g_catalog_sync_total);
            snprintf(line, sizeof(line), "SYNC %s: %u%%  %lu/%lu KB",
                     g_catalog_sync_stage == 2 ? "GAME INDEX 2/2" : "PROFILE DB 1/2",
                     percent, downloaded_kb, total_kb);
        } else {
            snprintf(line, sizeof(line), "SYNC %s: %lu KB RECEIVED",
                     g_catalog_sync_stage == 2 ? "GAME INDEX 2/2" : "PROFILE DB 1/2",
                     downloaded_kb);
        }
        draw_text(414.0f, 127.0f, 0x875be8ff, line);
    } else if (g_community_download_running) {
        unsigned long downloaded_kb =
            (unsigned long)((g_community_downloaded + 1023u) / 1024u);
        unsigned long total_kb =
            (unsigned long)((g_community_download_total + 1023u) / 1024u);
        if (g_community_download_total > 0) {
            unsigned int percent = (unsigned int)(
                (g_community_downloaded >= g_community_download_total) ? 100u :
                (g_community_downloaded * 100u) / g_community_download_total);
            snprintf(line, sizeof(line), "COMMUNITY FILE: %u%%  %lu/%lu KB",
                     percent, downloaded_kb, total_kb);
        } else {
            snprintf(line, sizeof(line), "COMMUNITY FILE: %lu KB RECEIVED",
                     downloaded_kb);
        }
        draw_text(414.0f, 127.0f, 0x875be8ff, line);
    } else {
        draw_text(414.0f, 127.0f, 0x77718fff,
                  "Review source, match and file type.");
    }
    draw_rounded_rect(412.0f, 153.0f, 197.0f, 25.0f, 7.0f,
                      g_catalog_ready ? 0xe8f5eced : 0xf3effbed);
    draw_rounded_rect(616.0f, 153.0f, 202.0f, 25.0f, 7.0f,
                      g_community_ready ? 0xe8f5eced : 0xf3effbed);
    set_ui_font_size(7, 11);
    draw_text(422.0f, 160.0f,
              g_catalog_ready ? 0x26794bff : 0x77718fff,
              g_catalog_ready ? "PROFILE CACHE: LOADED" :
                                "PROFILE CACHE: EMPTY");
    draw_text(624.0f, 160.0f,
              g_community_ready ? 0x26794bff : 0x77718fff,
              g_community_ready ? "GITHUB INDEX: LOADED" :
                                  "GITHUB INDEX: EMPTY");
    if (g_catalog_sync_running || g_community_download_running) {
        float progress = 0.0f;
        size_t downloaded = g_catalog_sync_running ?
                            g_catalog_sync_downloaded : g_community_downloaded;
        size_t total = g_catalog_sync_running ?
                       g_catalog_sync_total : g_community_download_total;
        draw_rounded_rect(414.0f, 182.0f, 404.0f, 4.0f, 2.0f,
                          0xe4deefff);
        if (total > 0) {
            progress = (float)downloaded / (float)total;
            if (progress > 1.0f) progress = 1.0f;
        } else if (g_animation_enabled) {
            progress = 0.12f + 0.22f *
                       (0.5f + 0.5f * sinf((float)g_frame_counter * 0.12f));
        } else {
            progress = 0.18f;
        }
        if (progress > 0.0f)
            draw_rounded_rect(414.0f, 182.0f, 404.0f * progress, 4.0f, 2.0f,
                              0x9568edff);
    }

    set_ui_font_size(7, 11);
    if (selected_profile && selected) {
        draw_soft_chip(414.0f, 190.0f, 182.0f,
                       "MATCHED PS3 PROFILE", 1);
        snprintf(line, sizeof(line), "GAME: %.54s", selected->title);
        draw_text(414.0f, 227.0f, 0x29243fff, line);
        snprintf(line, sizeof(line), "TITLE ID: %.18s", selected->title_id);
        draw_text(414.0f, 253.0f, 0x514b69ff, line);
        snprintf(line, sizeof(line), "VERSION: %.18s", selected_profile->app_ver);
        draw_text(414.0f, 277.0f, 0x514b69ff, line);
        draw_text(414.0f, 303.0f, 0x26794bff,
                  "MATCH: EXACT LOCAL GAME / PROFILE");
        snprintf(line, sizeof(line), "EFFECT: %.42s", selected_profile->effect);
        draw_text(414.0f, 328.0f, 0x514b69ff, line);
        draw_text(414.0f, 359.0f, 0xb27b35ff,
                  "APPLY: REVIEW + CONFIRM IN DETAILS");
        draw_text(414.0f, 390.0f, 0x875be8ff,
                  "CROSS: OPEN GAME DETAILS");
        draw_text(414.0f, 406.0f, 0x77718fff,
                  "No automatic game-file replacement.");
    } else if (selected_ps2_config) {
        draw_soft_chip(414.0f, 190.0f, 182.0f,
                       "PS2 COMPATIBILITY", 1);
        snprintf(line, sizeof(line), "GAME: %.48s", selected_ps2_config->title);
        draw_text(414.0f, 227.0f, 0x29243fff, line);
        snprintf(line, sizeof(line), "GAME ID: %.18s", selected_ps2_config->title_id);
        draw_text(414.0f, 253.0f, 0x514b69ff, line);
        snprintf(line, sizeof(line), "CONFIG TYPE: %.10s", selected_ps2_config->type);
        draw_text(414.0f, 277.0f, 0x514b69ff, line);
        draw_text(414.0f, 307.0f, 0xb27b35ff,
                  "APPLY: NOT A GRAPHICS PROFILE");
        draw_text(414.0f, 336.0f, 0x514b69ff,
                  "PS2 compatibility is game-specific.");
        draw_text(414.0f, 362.0f, 0x514b69ff,
                  "Not an HD texture or 1080p upgrade.");
        draw_text(414.0f, 394.0f, 0x875be8ff,
                  "CROSS: OPEN / REVIEW MATCH");
        draw_text(414.0f, 406.0f, 0x77718fff,
                  "Original ISO / classic is not overwritten.");
    } else if (selected_community) {
        const char *dot = strrchr(selected_community->path, '.');
        int title_version_match =
            selected && game_is_ps3(selected) &&
            selected->title_id[0] && selected->app_ver[0] &&
            strncmp(selected->path, "/dev_hdd0/", 10) == 0 &&
            ps3scale_community_file_matches_id_version(
                selected_community, selected);
        int exact_local_game =
            title_version_match &&
            ps3scale_community_file_matches_exact_game(
                selected_community, selected);
        int hash_ready = strlen(selected_community->git_blob_sha1) == 40;
        int can_download =
            exact_local_game && g_artemis_installed &&
            strcmp(selected_community->category, "GRAPHICS_FPS") == 0 &&
            dot && community_extension_is(dot, ".ncl") &&
            selected_community->file_size <= PS3SCALE_COMMUNITY_INDEX_DOWNLOAD_LIMIT;

        draw_soft_chip(414.0f, 190.0f, 182.0f,
                       "GITHUB COMMUNITY DATA", 1);
        set_ui_font_size(6, 9);
        draw_text(612.0f, 197.0f, 0x875be8ff,
                  can_download ? "X: INSTALL NCL" : "X: NOT READY");
        set_ui_font_size(7, 11);
        snprintf(line, sizeof(line), "GAME: %.54s", selected_community->title);
        draw_text(414.0f, 224.0f, 0x29243fff, line);
        snprintf(line, sizeof(line), "TITLE ID: %.18s", selected_community->title_id);
        draw_text(414.0f, 247.0f, 0x514b69ff, line);
        snprintf(line, sizeof(line), "CATEGORY: %.24s", selected_community->category);
        draw_text(414.0f, 269.0f, 0x514b69ff, line);
        snprintf(line, sizeof(line), "VERSION: %.18s | REGION: %s",
                 selected_community->app_ver[0] ?
                 selected_community->app_ver : "Not specified",
                 ps3scale_community_region_label(selected_community->title_id));
        draw_text(414.0f, 291.0f, 0x514b69ff, line);
        snprintf(line, sizeof(line), "SOURCE: %.38s", selected_community->repository);
        draw_text(414.0f, 313.0f, 0x514b69ff, line);
        snprintf(line, sizeof(line), "FILE: %.44s", selected_community->path);
        draw_text(414.0f, 335.0f, 0x514b69ff, line);
        draw_text(414.0f, 357.0f, hash_ready ? 0x26794bff : 0xb27b35ff,
                  hash_ready ? "GIT BLOB SHA-1: PRESENT" :
                               "HASH MISSING - REFRESH INDEX");
        if (exact_local_game) {
            draw_text(414.0f, 376.0f, 0x26794bff,
                      "MATCH: EXACT ID / VERSION + HASH");
        } else if (title_version_match) {
            draw_text(414.0f, 376.0f, 0xb27b35ff,
                      "MATCH: EXACT ID / VERSION; HASH MISSING");
        } else if (selected) {
            draw_text(414.0f, 376.0f, 0xb27b35ff,
                      "MATCH: REVIEW TITLE ID / VERSION");
        } else {
            draw_text(414.0f, 376.0f, 0xb27b35ff,
                      "SEARCH RESULT: NO LOCAL GAME MATCH");
        }
        snprintf(line, sizeof(line), "SIZE: %u KB | %s",
                 (unsigned)((selected_community->file_size + 1023u) / 1024u),
                 can_download ? "NCL GRAPHICS PATCH" : "NOT INSTALLABLE");
        draw_text(414.0f, 394.0f, 0x514b69ff, line);
        if (can_download) {
            draw_text(414.0f, 406.0f, 0x875be8ff,
                      "CROSS: INSTALL TO ARTEMIS USERLIST");
        } else if (!selected) {
            draw_text(414.0f, 406.0f, 0xb27b35ff,
                      "CROSS: SCAN THE EXACT GAME FIRST");
        } else if (!g_artemis_installed) {
            draw_text(414.0f, 406.0f, 0xb27b35ff,
                      "INSTALL ARTEMIS PS3 FIRST");
        } else if (strcmp(selected_community->category, "GRAPHICS_FPS") != 0) {
            draw_text(414.0f, 406.0f, 0xb27b35ff,
                      "NOT A GRAPHICS / FPS PATCH");
        } else if (!exact_local_game) {
            draw_text(414.0f, 406.0f, 0xb27b35ff,
                      "EXACT ID + VERSION + HASH REQUIRED");
        } else {
            draw_text(414.0f, 406.0f, 0xb27b35ff,
                      "ONLY NCL PATCH FILES ARE SUPPORTED");
        }
        draw_text(414.0f, 421.0f, 0xb27b35ff,
                  g_enhancement_persistent ?
                  "MODE: PERMANENT | SELECT IT IN ARTEMIS" :
                  "MODE: TEMPORARY | CLEARED NEXT PS3 SCALE START");
    } else {
        draw_text(414.0f, 215.0f, 0x77718fff,
                  g_online_result_count ?
                  "Select a result to review its match." :
                  "No results in this filter.");
        draw_text(414.0f, 241.0f, 0x514b69ff,
                  "Search by title, disc ID or title ID.");
        draw_text(414.0f, 273.0f, 0x875be8ff,
                  "TRIANGLE: REFRESH ONLINE DATABASE");
        draw_text(414.0f, 301.0f, 0x77718fff,
                  "Offline cache can still be browsed.");
    }

    /* Separate task status from controls to avoid text overlap at 720p. */
    if (g_online_message[0]) {
        draw_rounded_rect(20.0f, 426.0f, 814.0f, 22.0f, 6.0f,
                          0xf5effff0);
        set_ui_font_size(6, 9);
        snprintf(line, sizeof(line), "%.114s", g_online_message);
        draw_text(29.0f, 432.0f, 0x514b69ff, line);
    }
    set_ui_font_size(6, 9);
    draw_text(24.0f, PS3SCALE_UI_FOOTER_PRIMARY_Y, 0x514b69ff,
              "D-PAD SELECT | LEFT/RIGHT PAGE | L1/R1 PREV/NEXT | CROSS INSTALL/DETAILS");
    draw_text(24.0f, PS3SCALE_UI_FOOTER_SECONDARY_Y, 0x875be8ff,
              "TRIANGLE REFRESH | SELECT SEARCH | SQUARE FILTER | CIRCLE BACK");
    set_ui_font_size(14, 22);
}


static void draw_library_filter_item(float x, float y, float w,
                                      const char *label, size_t count,
                                      int selected, int focused)
{
    char number[16];
    draw_rounded_rect(x + 2.0f, y + 3.0f, w, 24.0f, 7.0f,
                      selected ? 0x6b5b8f30 : 0x60718f24);
    draw_rounded_rect(x, y, w, 24.0f, 7.0f,
                      selected ? 0xeee7ffed : 0xf8f6ffea);
    draw_rounded_outline(x, y, w, 24.0f, 7.0f,
                         selected ? 0x9568edff : 0xd9d1eaff);
    set_ui_font_size(6, 9);
    draw_text(x + 8.0f, y + 7.0f,
              selected ? 0x875be8ff : 0x514b69ff, label);
    snprintf(number, sizeof(number), "%u", (unsigned)count);
    draw_text(x + w - 25.0f, y + 7.0f, 0x77718fff, number);
    if (focused)
        draw_rounded_outline(x - 2.0f, y - 2.0f, w + 4.0f, 28.0f,
                             9.0f, 0x875be8ff);
}

static void draw_scan(void)
{
    static const char *filter_labels[5] = {
        "All Games", "PS3 Games", "PS2 Games", "Favorites", "Recently Added"
    };
    char line[192];
    size_t count;
    size_t start;
    size_t i;
    clamp_selection();
    count = library_count(g_library);
    start = ((size_t)(g_selected < 0 ? 0 : g_selected) /
             PS3SCALE_SCAN_GRID_COUNT) * PS3SCALE_SCAN_GRID_COUNT;
    draw_page_header("Game Library", "Browse, filter and select games from your local storage");

    draw_soft_card(20.0f, 88.0f, 150.0f, 356.0f, 0);
    set_ui_font_size(8, 12);
    draw_text(34.0f, 104.0f, 0x29243fff, "LIBRARY");
    for (i = 0; i < 5; i++) {
        draw_library_filter_item(28.0f, 130.0f + (float)i * 34.0f,
                                 134.0f, filter_labels[i],
                                 library_total_count((int)i),
                                 g_library == (int)i,
                                 g_scan_focus_sidebar &&
                                 g_scan_sidebar_selected == (int)i);
    }
    set_ui_font_size(6, 9);
    draw_text(34.0f, 338.0f, 0x77718fff, "LOCAL STORAGE");
    snprintf(line, sizeof(line), "%u TITLES DETECTED",
             (unsigned)library_total_count(LIBRARY_ALL));
    draw_text(34.0f, 358.0f, 0x514b69ff, line);
    snprintf(line, sizeof(line), "%u FAVORITES",
             (unsigned)library_total_count(LIBRARY_FAVORITES));
    draw_text(34.0f, 382.0f, 0x77718fff, line);
    draw_text(34.0f, 402.0f, 0x77718fff, "RECENT: FILE DATE");

    draw_soft_card(184.0f, 88.0f, 650.0f, 34.0f, 0);
    draw_circle_outline(202.0f, 105.0f, 6.0f, 0x514b69ff);
    draw_icon_line(207.0f, 110.0f, 212.0f, 115.0f, 0x514b69ff);
    set_ui_font_size(8, 12);
    snprintf(line, sizeof(line), "Search games...  %s",
             g_search[0] ? g_search : "");
    draw_text(222.0f, 99.0f, 0x77718fff, line);
    /* Small grid glyph matches the pearl/violet home icon style. */
    draw_rounded_rect(782.0f, 94.0f, 34.0f, 22.0f, 6.0f,
                      0xf0e9ffed);
    draw_rounded_outline(782.0f, 94.0f, 34.0f, 22.0f, 6.0f,
                         0xb99affff);
    draw_solid_rect(791.0f, 100.0f, 6.0f, 4.0f, 0x875be8ff);
    draw_solid_rect(800.0f, 100.0f, 6.0f, 4.0f, 0x875be8ff);
    draw_solid_rect(791.0f, 107.0f, 6.0f, 4.0f, 0x875be8ff);
    draw_solid_rect(800.0f, 107.0f, 6.0f, 4.0f, 0x875be8ff);

    for (i = 0; i < PS3SCALE_SCAN_GRID_COUNT && start + i < count; i++) {
        const ps3scale_game_t *game = library_game_at(start + i);
        float focus = g_scan_card_focus[i];
        float scale = 1.0f + focus * 0.035f;
        float card_w = 150.0f * scale;
        float card_h = 148.0f * scale;
        float x = 184.0f + (float)(i % 4) * 160.0f;
        float y = 132.0f + (float)(i / 4) * 154.0f;
        float card_x = x - (card_w - 150.0f) * 0.5f;
        float card_y = y - (card_h - 148.0f) * 0.5f - focus * 4.0f;
        float cover_x = card_x + 7.0f * scale;
        float cover_y = card_y + 7.0f * scale;
        float cover_w = 136.0f * scale;
        float cover_h = 96.0f * scale;
        u32 tile_color = (i % 3 == 0) ? 0xe9e1f8ff :
                         (i % 3 == 1) ? 0xe4e9f5ff : 0xf0e6f3ff;
        if (!game) continue;

        draw_library_animated_card(card_x, card_y, card_w, card_h, focus);
        /* Scale cover art with its card so selection never clips the artwork. */
        draw_rounded_rect(card_x + 5.0f * scale, card_y + 5.0f * scale,
                          140.0f * scale, 100.0f * scale, 7.0f * scale,
                          ui_blend_color(0xe6e0efff, 0xc4a6f3ff, focus));
        if (g_scan_cover_textures[i]) {
            ya2d_drawTextureEx(g_scan_cover_textures[i], cover_x, cover_y,
                               0.0f, cover_w, cover_h);
        } else {
            char initial[2];
            draw_solid_rect(cover_x, cover_y, cover_w, cover_h, tile_color);
            draw_solid_rect(cover_x, cover_y, cover_w, 4.0f * scale,
                            0xb69affff);
            initial[0] = game->title[0] ?
                (char)toupper((unsigned char)game->title[0]) : '?';
            initial[1] = '\0';
            set_ui_font_size(28, 42);
            draw_text_centered(cover_x + 3.0f * scale,
                               cover_y + 31.0f * scale,
                               cover_w - 6.0f * scale,
                               0x875be8ff, initial);
        }

        /* Platform badge stays anchored to the cover's lower-right corner. */
        draw_rounded_rect(card_x + 107.0f * scale,
                          card_y + 82.0f * scale,
                          30.0f * scale, 17.0f * scale, 5.0f * scale,
                          0x5f299de8);
        set_ui_font_size(7, 11);
        draw_text(card_x + 109.0f * scale, card_y + 84.0f * scale,
                  0xfdfaffff, game_is_ps3(game) ? "PS3" : "PS2");
        set_ui_font_size(7, 11);
        snprintf(line, sizeof(line), "%.17s", game->title);
        draw_text(card_x + 9.0f * scale, card_y + 110.0f * scale,
                  0x29243fff, line);
        set_ui_font_size(6, 9);
        draw_text(card_x + 9.0f * scale, card_y + 130.0f * scale,
                  0x77718fff,
                  game_is_ps3(game) ? "PlayStation 3" : "PlayStation 2");
    }

    if (count == 0) {
        draw_soft_card(184.0f, 132.0f, 650.0f, 296.0f, 0);
        set_ui_font_size(10, 15);
        draw_text_centered(204.0f, 252.0f, 610.0f,
                           0x77718fff, "No games match this filter.");
    }
    set_ui_font_size(7, 11);
    snprintf(line, sizeof(line), "%u GAMES FOUND%s", (unsigned)count,
             g_games.truncated ? " *" : "");
    draw_text(188.0f, 440.0f, 0x514b69ff, line);
    if (count > 0) {
        size_t pages = (count + PS3SCALE_SCAN_GRID_COUNT - 1u) /
                       PS3SCALE_SCAN_GRID_COUNT;
        size_t page = start / PS3SCALE_SCAN_GRID_COUNT + 1u;
        snprintf(line, sizeof(line), "PAGE %u / %u",
                 (unsigned)page, (unsigned)pages);
        draw_text(727.0f, 440.0f, 0x875be8ff, line);
    }
    if (g_scan_message[0])
        draw_text(360.0f, 440.0f, 0x875be8ff, g_scan_message);
    else if (g_games.truncated)
        draw_text(360.0f, 440.0f, 0xffcc66ff,
                  "WARNING: LIBRARY LIMIT REACHED (256 TITLES)");
    draw_text(24.0f, PS3SCALE_UI_FOOTER_PRIMARY_Y, 0x77718fff,
              "D-PAD: MOVE   |   LEFT EDGE: FILTERS   |   CROSS: DETAILS   |   CIRCLE: BACK");
    draw_text(24.0f, PS3SCALE_UI_FOOTER_SECONDARY_Y, 0x875be8ff,
              "SQUARE: FAVORITE   |   SELECT+SQUARE: RESCAN   |   START / SELECT: SEARCH");
    set_ui_font_size(14, 22);
}

static void release_selected_preview(void)
{
    if (g_preview_before_texture)
        ya2d_freeTexture(g_preview_before_texture);
    if (g_preview_after_texture)
        ya2d_freeTexture(g_preview_after_texture);
    g_preview_before_texture = NULL;
    g_preview_after_texture = NULL;
    g_preview_key[0] = '\0';
}

static void update_selected_preview(void)
{
    const ps3scale_game_t *game = selected_game();
    char key[32];
    char before_path[PS3SCALE_PATH_MAX];
    char after_path[PS3SCALE_PATH_MAX];
    int written;

    if (!game || !game_is_ps3(game) ||
        !game->title_id[0] || !game->app_ver[0]) {
        release_selected_preview();
        return;
    }

    if (snprintf(key, sizeof(key), "%s_%s",
                 game->title_id, game->app_ver) >= (int)sizeof(key)) {
        release_selected_preview();
        return;
    }
    if (strcmp(key, g_preview_key) == 0)
        return;

    release_selected_preview();
    snprintf(g_preview_key, sizeof(g_preview_key), "%s", key);
    written = snprintf(before_path, sizeof(before_path),
                       "%s/%s_before.png", PS3SCALE_PREVIEW_DIR, key);
    if (written < 0 || (size_t)written >= sizeof(before_path))
        return;
    written = snprintf(after_path, sizeof(after_path),
                       "%s/%s_after.png", PS3SCALE_PREVIEW_DIR, key);
    if (written < 0 || (size_t)written >= sizeof(after_path))
        return;

    /* Missing or unexpectedly large preview files must not be handed to the
       PNG decoder on the controller/render thread. Require a bounded real pair. */
    if (!image_file_is_safe(before_path, PS3SCALE_PREVIEW_MAX_BYTES) ||
        !image_file_is_safe(after_path, PS3SCALE_PREVIEW_MAX_BYTES))
        return;

    g_preview_before_texture = ya2d_loadPNGfromFile(before_path);
    g_preview_after_texture = ya2d_loadPNGfromFile(after_path);
    /* Never show a one-sided or invented comparison. Both images are required. */
    if (!g_preview_before_texture || !g_preview_after_texture) {
        if (g_preview_before_texture)
            ya2d_freeTexture(g_preview_before_texture);
        if (g_preview_after_texture)
            ya2d_freeTexture(g_preview_after_texture);
        g_preview_before_texture = NULL;
        g_preview_after_texture = NULL;
    }
}

static void draw_details(void)
{
    char line[192];
    const ps3scale_game_t *game = selected_game();
    const ps3scale_profile_t *profile = profile_for_game(game);
    const ps3scale_ps2_config_entry_t *ps2_config = ps2_config_for_game(game);
    int ps2_config_exists = g_details_cache_valid ? g_details_ps2_config_exists : 0;
    int script_state = g_details_cache_valid ? g_details_script_state : 0;

    draw_page_header("Game Details", "Review compatibility and available enhancements");
    if (!game) {
        draw_soft_card(24.0f, 92.0f, 806.0f, 340.0f, 0);
        draw_text(52.0f, 128.0f, 0x29243fff, "No game selected.");
        draw_text(52.0f, PS3SCALE_UI_FOOTER_SINGLE_Y, 0x77718fff, "CIRCLE / SELECT / LEFT: BACK");
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
    /* Status pill tells the user what is actually available. */
    draw_rounded_rect(203.0f, 270.0f, 290.0f, 21.0f, 7.0f,
                      (profile || ps2_config) ? 0xe5f5eaff : 0xfff3e5ff);
    set_ui_font_size(7, 11);
    draw_text(212.0f, 275.0f,
              (profile || ps2_config) ? 0x26794bff : 0xb27b35ff,
              profile ? ((strcmp(profile->trust, "B_COMMUNITY") == 0 &&
                           profile->patch_count == 1) ?
                          "MATCHED PROFILE - AUTO APPLY ON CROSS" :
                          "MANUAL PROFILE - REVIEW FIRST") :
              ps2_config ? "MATCHED PS2 CONFIG - REVIEW FIRST" :
              (game->kind == PS3SCALE_GAME_PS2_ISO &&
               !ps2_config_preferred_type() ? "HARDWARE NOT IDENTIFIED" :
                                              "NO MATCHED FILE - PLAY UNMODIFIED"));
    set_ui_font_size(11, 17);
    snprintf(line, sizeof(line), "%.26s", game->title);
    draw_text(204.0f, 110.0f, 0x29243fff, line);
    set_ui_font_size(7, 11);
    snprintf(line, sizeof(line), "Platform: %s",
             game_is_ps3(game) ? "PlayStation 3" : "PlayStation 2");
    draw_text(204.0f, 150.0f, 0x514b69ff, line);
    snprintf(line, sizeof(line), "Game ID: %.18s",
             game->title_id[0] ? game->title_id :
             (game->disc_id[0] ? game->disc_id : "Not read"));
    draw_text(204.0f, 176.0f, 0x514b69ff, line);
    snprintf(line, sizeof(line), "Region: %.18s",
             game->region[0] ? game->region : "UNKNOWN");
    draw_text(204.0f, 202.0f, 0x514b69ff, line);
    snprintf(line, sizeof(line), "Version: %.16s",
             game_is_ps3(game) ?
             (game->app_ver[0] ? game->app_ver : "Unknown - rescan") :
             "PS2 configuration");
    draw_text(204.0f, 224.0f, 0x514b69ff, line);
    snprintf(line, sizeof(line), "Location: %.25s", game->path);
    draw_text(204.0f, 246.0f, 0x77718fff, line);
    draw_text(204.0f, 300.0f, 0x29243fff, "Enhancement Status");
    if (game_is_ps3(game)) {
        if (profile) {
            snprintf(line, sizeof(line), "Profile: %.30s", profile->effect);
            draw_text(204.0f, 320.0f, 0x2c9f8eff, line);
            if (script_state == -2)
                draw_text(204.0f, 346.0f, 0xffcc66ff, "Profile script is stale.");
            else if (script_state == -1)
                draw_text(204.0f, 346.0f, 0xffcc66ff, "User script is protected.");
            else if (profile->patch_count != 1)
                draw_text(204.0f, 346.0f, 0xffcc66ff, "Manual-only profile.");
            else
                draw_text(204.0f, 346.0f, 0x2c9f8eff, "Exact profile available.");
        } else {
            /* NO PROFILE FOR THIS TITLE. GAME WILL START UNMODIFIED. */
            draw_text(204.0f, 320.0f, 0xffcc66ff,
                      "NO PROFILE FOR THIS TITLE.");
            draw_text(204.0f, 346.0f, 0x77718fff,
                      "GAME WILL START UNMODIFIED.");
        }
    } else if (ps2_config) {
        snprintf(line, sizeof(line), "Community config: %.10s", ps2_config->type);
        draw_text(204.0f, 320.0f, 0x2c9f8eff, line);
        draw_text(204.0f, 346.0f,
                  ps2_config_exists ? 0xffcc66ff : 0x2c9f8eff,
                  ps2_config_exists ? "Config already installed." :
                                      "Verified config available.");
    } else if (game->kind == PS3SCALE_GAME_PS2_ISO &&
               !ps2_config_preferred_type()) {
        draw_text(204.0f, 320.0f, 0xffcc66ff,
                  "CONSOLE HARDWARE NOT IDENTIFIED.");
        draw_text(204.0f, 346.0f, 0x77718fff,
                  "PS2 ISO CONFIG MATCHING DISABLED.");
    } else {
        draw_text(204.0f, 320.0f, 0xffcc66ff,
                  "No verified PS2 config matches this ID.");
        draw_text(204.0f, 346.0f, 0x77718fff,
                  "Original game image will not be modified.");
    }

    draw_soft_card(526.0f, 92.0f, 308.0f, 292.0f, 0);
    if (g_ps2_config_download_running) {
        unsigned long downloaded_kb =
            (unsigned long)((g_ps2_config_downloaded + 1023u) / 1024u);
        unsigned long total_kb =
            (unsigned long)((g_ps2_config_download_total + 1023u) / 1024u);
        float progress = 0.0f;
        if (g_ps2_config_download_total > 0) {
            unsigned int percent = (unsigned int)(
                (g_ps2_config_downloaded >= g_ps2_config_download_total) ? 100u :
                (g_ps2_config_downloaded * 100u) / g_ps2_config_download_total);
            snprintf(line, sizeof(line), "PS2 CONFIG: %u%%  %lu/%lu KB",
                     percent, downloaded_kb, total_kb);
            progress = (float)g_ps2_config_downloaded /
                       (float)g_ps2_config_download_total;
            if (progress > 1.0f) progress = 1.0f;
        } else {
            snprintf(line, sizeof(line), "PS2 CONFIG: %lu KB RECEIVED",
                     downloaded_kb);
            progress = g_animation_enabled ?
                0.12f + 0.22f * (0.5f + 0.5f *
                    sinf((float)g_frame_counter * 0.12f)) : 0.18f;
        }
        set_ui_font_size(6, 9);
        draw_text(544.0f, 362.0f, 0x875be8ff, line);
        draw_rounded_rect(544.0f, 376.0f, 272.0f, 4.0f, 2.0f,
                          0xe4deefff);
        if (progress > 0.0f)
            draw_rounded_rect(544.0f, 376.0f, 272.0f * progress, 4.0f, 2.0f,
                              0x9568edff);
    }
    set_ui_font_size(9, 14);
    draw_text(544.0f, 108.0f, 0x29243fff, "REAL GAME COMPARISON");
    set_ui_font_size(6, 9);
    if (g_preview_before_texture && g_preview_after_texture) {
        ya2d_drawTextureEx(g_preview_before_texture, 542.0f, 136.0f,
                           0.0f, 132.0f, 84.0f);
        ya2d_drawTextureEx(g_preview_after_texture, 680.0f, 136.0f,
                           0.0f, 136.0f, 84.0f);
        draw_text(554.0f, 228.0f, 0x514b69ff, "ORIGINAL");
        draw_text(744.0f, 228.0f, 0x875be8ff, "PATCHED");
    } else {
        draw_rounded_rect(542.0f, 136.0f, 274.0f, 84.0f, 7.0f,
                          0xf3efffff);
        draw_rounded_outline(542.0f, 136.0f, 274.0f, 84.0f, 7.0f,
                             0xd9d1eaff);
        draw_text(558.0f, 158.0f, 0xb27b35ff,
                  "NO MATCHED SCREENSHOTS");
        draw_text(558.0f, 178.0f, 0x514b69ff,
                  "FOR THIS GAME VERSION");
        draw_text(558.0f, 198.0f, 0x77718fff,
                  "Add a real pair to /USRDIR/previews.");
    }
    draw_soft_chip(542.0f, 246.0f, 132.0f, "Enhancement Info", 1);
    draw_soft_chip(680.0f, 246.0f, 136.0f, "Compatibility", 0);
    /* Don't promise universal 1080p, HD textures or performance gains:
       PS3 patches and PS2 compatibility configs do different jobs. */
    set_ui_font_size(6, 9);
    if (game_is_ps3(game) && profile) {
        draw_text(544.0f, 285.0f, 0x26794bff,
                  "EXACT TITLE ID / APP VERSION MATCH");
        snprintf(line, sizeof(line), "Profile: %.27s", profile->effect);
        draw_text(544.0f, 304.0f, 0x514b69ff, line);
        draw_text(544.0f, 323.0f, 0x514b69ff,
                  "Results depend on the exact game version.");
        draw_text(544.0f, 342.0f, 0x875be8ff,
                  g_preview_before_texture && g_preview_after_texture ?
                  "Before/after pair loaded." :
                  "No matched before/after images are available.");
    } else if (!game_is_ps3(game) && ps2_config) {
        draw_text(544.0f, 285.0f, 0x26794bff,
                  "PS2 CONFIG / ENHANCEMENT AVAILABLE");
        draw_text(544.0f, 304.0f, 0x514b69ff,
                  "Game-specific timing and compatibility fixes.");
        draw_text(544.0f, 323.0f, 0x514b69ff,
                  "No universal 60 FPS or HD texture unlock.");
        draw_text(544.0f, 342.0f, 0x875be8ff,
                  "XMB upscaler is a separate display setting.");
    } else if (game->kind == PS3SCALE_GAME_PS2_ISO &&
               !ps2_config_preferred_type()) {
        draw_text(544.0f, 290.0f, 0xb27b35ff,
                  "CONSOLE HARDWARE NOT IDENTIFIED");
        draw_text(544.0f, 311.0f, 0x514b69ff,
                  "PS2 ISO CONFIG MATCHING IS DISABLED.");
        draw_text(544.0f, 332.0f, 0x77718fff,
                  "HARDWARE DETECTION IS REQUIRED.");
    } else {
        draw_text(544.0f, 290.0f, 0xb27b35ff,
                  "NO VERIFIED ENHANCEMENT AVAILABLE");
        draw_text(544.0f, 311.0f, 0x514b69ff,
                  "The game will start without changes.");
        draw_text(544.0f, 334.0f, 0x875be8ff,
                  "No matched comparison images are available.");
    }

    draw_text(544.0f, 362.0f, 0x77718fff,
              "This is NOT an HD texture replacement.");

    draw_soft_button(20.0f, 400.0f, 250.0f, 44.0f,
                     game_is_ps3(game) ?
                     (profile ? "TRIANGLE: ARM PROFILE" : "NO EXACT FPS PROFILE") :
                     (ps2_config ? "TRIANGLE: DOWNLOAD CONFIG" : "NO CONFIG MATCH"),
                     1);
    draw_soft_button(282.0f, 400.0f, 250.0f, 44.0f,
                     game_is_ps3(game) ? "SQUARE: FPS CONTROL" :
                                         "SQUARE: PS2 ENHANCER", 0);
    draw_soft_button(544.0f, 400.0f, 290.0f, 44.0f,
                     (game_is_ps3(game) && profile) ?
                      ((strcmp(profile->trust, "B_COMMUNITY") == 0 &&
                        profile->patch_count == 1) ?
                       ((script_state == -1 || script_state == -2) ?
                        "CROSS: PROFILE BLOCKED" :
                        (script_state == 1 ? "CROSS: START WITH PROFILE" :
                                             "CROSS: APPLY + START")) :
                       "CROSS: MANUAL REVIEW") : "CROSS: START GAME", 0);
    set_ui_font_size(7, 11);
    draw_text(24.0f, PS3SCALE_UI_FOOTER_PRIMARY_Y, 0x77718fff,
              g_profile_confirm_pending ?
              "COMMUNITY PATCH: TRIANGLE CONFIRMS; CIRCLE CANCELS." :
              g_details_message);
    draw_text(24.0f, PS3SCALE_UI_FOOTER_SECONDARY_Y, 0x875be8ff,
              "CROSS / START: AUTO APPLY + PLAY WHEN MATCHED | TRIANGLE: ARM PROFILE | CIRCLE / SELECT / LEFT: BACK");
    set_ui_font_size(14, 22);
}


static void draw_ps2_config_manager(void)
{
    char line[192];
    const ps3scale_game_t *game = selected_game();
    const ps3scale_ps2_config_entry_t *entry = ps2_config_for_game(game);
    const int can_generate =
        game && game->kind == PS3SCALE_GAME_PS2_ISO;
    const int netemu_supported = ps2_netemu_iso_supported();
    const int local_profile_supported = can_generate && netemu_supported;
    const char *fps_patch_label =
        g_ps2_patch_confirm_pending &&
        g_ps2_patch_confirm_kind == PS3SCALE_PS2_PATCH_FPS ?
        "CONFIRM 60 FPS PATCH" :
        (!can_generate ? "ISO PATCH ONLY" :
         (!netemu_supported ? "NETEMU HARDWARE REQUIRED" :
          (g_ps2_patch_probe_valid < 0 ? "BOOT ELF NOT READABLE" :
           (g_ps2_patch_fps_available ? "APPLY EXPERIMENTAL 60 FPS PATCH" :
                                        "NO EXACT 60 FPS PATCH"))));
    const char *widescreen_patch_label =
        g_ps2_patch_confirm_pending &&
        g_ps2_patch_confirm_kind == PS3SCALE_PS2_PATCH_WIDESCREEN ?
        "CONFIRM 16:9 PATCH" :
        (!can_generate ? "ISO PATCH ONLY" :
         (!netemu_supported ? "NETEMU HARDWARE REQUIRED" :
          (g_ps2_patch_probe_valid < 0 ? "BOOT ELF NOT READABLE" :
           (g_ps2_patch_widescreen_available ? "APPLY 16:9 WIDESCREEN PATCH" :
                                               "NO EXACT 16:9 PATCH"))));
    const char *p480_patch_label =
        g_ps2_patch_confirm_pending &&
        g_ps2_patch_confirm_kind == PS3SCALE_PS2_PATCH_480P ?
        "CONFIRM 480P PATCH" :
        (!can_generate ? "ISO PATCH ONLY" :
         (!netemu_supported ? "NETEMU HARDWARE REQUIRED" :
          (g_ps2_patch_probe_valid < 0 ? "BOOT ELF NOT READABLE" :
           (g_ps2_patch_480p_available ? "APPLY 480P OUTPUT PATCH" :
                                         "NO EXACT 480P PATCH"))));
    const char *config_state =
        g_ps2_config_manager_target_state == 1 ? "PRESENT" :
        (g_ps2_config_manager_target_state == 0 ? "MISSING" :
                                                  "INVALID / TOO LARGE");
    const char *backup_state =
        g_ps2_config_manager_backup_state == 1 ? "SAVED" :
        (g_ps2_config_manager_backup_state == 0 ? "NOT SAVED" : "INVALID");
    const char *rollback_state =
        g_ps2_config_manager_rollback_state == 1 ? "AVAILABLE" :
        (g_ps2_config_manager_rollback_state == 0 ? "NONE" : "INVALID");
    const char *target_name = strrchr(g_ps2_config_manager_target, '/');

    target_name = target_name ? target_name + 1 :
                                 g_ps2_config_manager_target;

    draw_page_header(
        "PS2 ENHANCER",
        "Per-game netemu CONFIG tools; ISO remains untouched.");
    draw_soft_card(24.0f, 92.0f, 806.0f, 340.0f, 0);

    set_ui_font_size(10, 16);
    snprintf(line, sizeof(line), "Game: %.48s",
             game ? game->title : "No PS2 game selected");
    draw_text(48.0f, 112.0f, 0x29243fff, line);
    set_ui_font_size(7, 11);

    snprintf(line, sizeof(line), "CONFIG file: %s", config_state);
    draw_text(48.0f, 142.0f,
              g_ps2_config_manager_target_state == 1 ?
              0x26794bff : 0xb27b35ff, line);
    snprintf(line, sizeof(line), "Saved backup: %s", backup_state);
    draw_text(48.0f, 162.0f,
              g_ps2_config_manager_backup_state == 1 ?
              0x26794bff : 0x77718fff, line);
    snprintf(line, sizeof(line), "Rollback: %s", rollback_state);
    draw_text(48.0f, 182.0f,
              g_ps2_config_manager_rollback_state == 1 ?
              0x26794bff : 0x77718fff, line);

    snprintf(line, sizeof(line), "Match: %.42s",
             entry ? entry->title :
             (game && game->kind == PS3SCALE_GAME_PS2_ISO &&
              !ps2_config_preferred_type() ? "HARDWARE NOT IDENTIFIED" :
                                             "NO EXACT GAME CONFIG"));
    draw_text(430.0f, 142.0f,
              entry ? 0x26794bff : 0xb27b35ff, line);
    snprintf(line, sizeof(line), "ID: %s | Type: %s",
             entry ? entry->title_id : "--",
             entry ? entry->type : "--");
    draw_text(430.0f, 162.0f, 0x514b69ff, line);
    if (g_ps2_config_manager_target_state == 1) {
        snprintf(line, sizeof(line), "CONFIG size: %lu bytes",
                 (unsigned long)g_ps2_config_manager_target_size);
        draw_text(430.0f, 182.0f, 0x514b69ff, line);
    }
    if (target_name && target_name[0]) {
        snprintf(line, sizeof(line), "Target: %.52s", target_name);
        draw_text(48.0f, 204.0f, 0x514b69ff, line);
    }
    if (can_generate && !netemu_supported) {
        snprintf(line, sizeof(line), "PATCH MATCH: NETEMU HARDWARE REQUIRED");
    } else if (can_generate && g_ps2_patch_probe_valid == 1) {
        snprintf(line, sizeof(line), "PATCH MATCH: FPS %s | 16:9 %s | 480P %s",
                 g_ps2_patch_fps_available ? "YES" : "NO",
                 g_ps2_patch_widescreen_available ? "YES" : "NO",
                 g_ps2_patch_480p_available ? "YES" : "NO");
    } else {
        snprintf(line, sizeof(line), can_generate ?
                 "PATCH MATCH: BOOT ELF CHECK FAILED" :
                 "PATCH MATCH: PS2 ISO REQUIRED");
    }
    draw_text(430.0f, 204.0f, 0xb27b35ff, line);
    draw_text(48.0f, 220.0f, local_profile_supported ? 0x26794bff : 0xb27b35ff,
              can_generate ?
              (netemu_supported ?
               "NETEMU OPTIONS ENABLED FOR THIS DEVICE." :
               "NETEMU OPTIONS DISABLED: SLIM / SUPER SLIM REQUIRED.") :
              "LOCAL PROFILES AND PATCHES REQUIRE A PS2 ISO.");
    if (can_generate && g_ps2_patch_probe_valid == 1) {
        snprintf(line, sizeof(line), "EXACT MATCH: %.15s | ELF HASH: %08lX",
                 g_ps2_patch_serial, (unsigned long)g_ps2_patch_elf_hash);
        draw_text(430.0f, 220.0f, 0x514b69ff, line);
    } else {
        draw_text(430.0f, 220.0f, 0xb27b35ff,
                  "EXACT MATCH: UNAVAILABLE");
    }

    draw_soft_card(48.0f, 232.0f, 352.0f, 38.0f,
                   g_ps2_config_manager_selection == 0);
    draw_text(62.0f, 238.0f, local_profile_supported ? 0x29243fff : 0x77718fff, "L2H PERFORMANCE (0x46)");
    draw_text(62.0f, 253.0f, 0x77718fff,
              "May help titles using L2H downloads.");
    draw_text(350.0f, 244.0f,
              local_profile_supported && (g_ps2_enhancement_flags & PS3SCALE_PS2_CFG_ENABLE_L2H) ?
              0x26794bff : 0x77718fff,
              (g_ps2_enhancement_flags & PS3SCALE_PS2_CFG_ENABLE_L2H) ?
              "ON" : "OFF");

    draw_soft_card(48.0f, 275.0f, 352.0f, 38.0f,
                   g_ps2_config_manager_selection == 1);
    draw_text(62.0f, 281.0f, local_profile_supported ? 0x29243fff : 0x77718fff, "SHARPER PIXELS (0x44)");
    draw_text(62.0f, 296.0f, 0x77718fff,
              "Disables smoothing; aliasing may increase.");
    draw_text(350.0f, 287.0f,
              local_profile_supported && (g_ps2_enhancement_flags & PS3SCALE_PS2_CFG_DISABLE_SMOOTHING) ?
              0x26794bff : 0x77718fff,
              (g_ps2_enhancement_flags & PS3SCALE_PS2_CFG_DISABLE_SMOOTHING) ?
              "ON" : "OFF");

    draw_soft_card(48.0f, 318.0f, 352.0f, 38.0f,
                   g_ps2_config_manager_selection == 2);
    draw_text(62.0f, 324.0f, local_profile_supported ? 0x29243fff : 0x77718fff, "FRAME PACING (0x48)");
    draw_text(62.0f, 339.0f, 0x77718fff,
              "Conservative VSYNC timing; experimental.");
    draw_text(350.0f, 330.0f,
              local_profile_supported && (g_ps2_enhancement_flags & PS3SCALE_PS2_CFG_FRAME_PACING) ?
              0xb27b35ff : 0x77718fff,
              (g_ps2_enhancement_flags & PS3SCALE_PS2_CFG_FRAME_PACING) ?
              "ON" : "OFF");

    draw_soft_card(48.0f, 361.0f, 352.0f, 38.0f,
                   g_ps2_config_manager_selection == 3);
    draw_text(62.0f, 367.0f, local_profile_supported ? 0x29243fff : 0x77718fff, "PRESSURE SENSITIVITY (0x50)");
    draw_text(62.0f, 382.0f, 0x77718fff,
              "Enables analog pressure-sensitive button input.");
    draw_text(350.0f, 373.0f,
              local_profile_supported && (g_ps2_enhancement_flags & PS3SCALE_PS2_CFG_ENABLE_PRESSURE) ?
              0x26794bff : 0x77718fff,
              (g_ps2_enhancement_flags & PS3SCALE_PS2_CFG_ENABLE_PRESSURE) ?
              "ON" : "OFF");

    draw_soft_button(430.0f, 218.0f, 350.0f, 22.0f,
                     can_generate ?
                     (!netemu_supported ? "NETEMU HARDWARE REQUIRED" :
                      (g_ps2_config_manager_target_state == 1 ?
                       "EXISTING CONFIG PRESERVED" : "APPLY LOCAL PROFILE")) :
                     "ISO PROFILE ONLY",
                     g_ps2_config_manager_selection == 4);
    draw_soft_button(430.0f, 246.0f, 350.0f, 22.0f,
                     fps_patch_label,
                     g_ps2_config_manager_selection == 5);
    draw_soft_button(430.0f, 274.0f, 350.0f, 22.0f,
                     widescreen_patch_label,
                     g_ps2_config_manager_selection == 6);
    draw_soft_button(430.0f, 302.0f, 350.0f, 22.0f,
                     p480_patch_label,
                     g_ps2_config_manager_selection == 7);
    draw_soft_button(430.0f, 330.0f, 350.0f, 22.0f,
                     entry ? "DOWNLOAD EXACT GAME CONFIG" :
                     (game && game->kind == PS3SCALE_GAME_PS2_ISO &&
                      !ps2_config_preferred_type() ? "HARDWARE NOT IDENTIFIED" :
                                                     "NO EXACT GAME CONFIG"),
                     g_ps2_config_manager_selection == 8);
    draw_soft_button(430.0f, 358.0f, 350.0f, 22.0f,
                     "BACK UP CURRENT CONFIG",
                     g_ps2_config_manager_selection == 9);
    draw_soft_button(430.0f, 386.0f, 350.0f, 22.0f,
                     "RESTORE SAVED BACKUP",
                     g_ps2_config_manager_selection == 10);

    set_ui_font_size(7, 11);
    if (g_ps2_config_download_running) {
        unsigned int percent = 0;
        unsigned long downloaded_kb =
            (unsigned long)((g_ps2_config_downloaded + 1023u) / 1024u);
        unsigned long total_kb =
            (unsigned long)((g_ps2_config_download_total + 1023u) / 1024u);
        float progress = 0.12f;

        if (g_ps2_config_download_total > 0) {
            percent = (unsigned int)(
                g_ps2_config_downloaded >= g_ps2_config_download_total ?
                100u :
                (g_ps2_config_downloaded * 100u) /
                g_ps2_config_download_total);
            progress = (float)g_ps2_config_downloaded /
                       (float)g_ps2_config_download_total;
            if (progress > 1.0f)
                progress = 1.0f;
            snprintf(line, sizeof(line), "DOWNLOAD: %u%%  %lu/%lu KB",
                     percent, downloaded_kb, total_kb);
        } else {
            snprintf(line, sizeof(line), "DOWNLOAD: %lu KB RECEIVED",
                     downloaded_kb);
        }
        draw_text(48.0f, 410.0f, 0x875be8ff, line);
        draw_rounded_rect(48.0f, 422.0f, 730.0f, 4.0f, 2.0f,
                          0xe4deefff);
        if (progress > 0.0f)
            draw_rounded_rect(48.0f, 422.0f, 730.0f * progress, 4.0f,
                              2.0f, 0x9568edff);
    } else {
        draw_text(48.0f, 410.0f, 0x875be8ff,
                  g_ps2_config_manager_message);
    }

    if (g_ps2_enhancer_guide_open) {
        draw_rounded_rect(24.0f, 92.0f, 806.0f, 340.0f, 14.0f,
                          0xfaf8ffff);
        draw_rounded_outline(24.0f, 92.0f, 806.0f, 340.0f, 14.0f,
                             0xd9d1eaff);
        set_ui_font_size(10, 16);
        draw_text(58.0f, 118.0f, 0x29243fff,
                  "PS3 XMB IMAGE SETTINGS");
        set_ui_font_size(7, 11);
        draw_text(58.0f, 154.0f, 0x514b69ff,
                  "Settings > Game Settings > PS / PS2 Upscaler");
        draw_text(58.0f, 178.0f, 0x26794bff,
                  "Normal: upscale while preserving the original aspect ratio.");
        draw_text(58.0f, 202.0f, 0xb27b35ff,
                  "Full Screen: stretch to fill the screen; aspect ratio changes.");
        draw_text(58.0f, 242.0f, 0x514b69ff,
                  "PS / PS2 Smoothing");
        draw_text(58.0f, 266.0f, 0x26794bff,
                  "On: soften jagged edges; may look blurrier.");
        draw_text(58.0f, 290.0f, 0x514b69ff,
                  "Off: keep pixels sharper; aliasing may be more visible.");
        draw_text(58.0f, 330.0f, 0x514b69ff,
                  "Display Settings > Video Output Settings: choose a supported 720p / 1080p output.");
        draw_text(58.0f, 354.0f, 0x514b69ff,
                  "This upscales the output; it does not render the PS2 game natively at HD.");
        draw_text(58.0f, 390.0f, 0x875be8ff,
                  "These are global XMB settings. PS3 SCALE cannot change them automatically.");
    }

    draw_text(24.0f, PS3SCALE_UI_FOOTER_SINGLE_Y, 0x77718fff,
              g_ps2_enhancer_guide_open ?
              "TRIANGLE / CIRCLE: CLOSE DISPLAY GUIDE" :
              (g_ps2_patch_confirm_pending ?
               "EXPERIMENTAL PATCH: CROSS CONFIRMS | CIRCLE CANCELS" :
               "UP/DOWN: SELECT | CROSS: TOGGLE / REVIEW PATCH | TRIANGLE: DISPLAY GUIDE | CIRCLE: BACK"));
    set_ui_font_size(14, 22);
}

static void draw_fps_control(void)
{
    char line[192];
    const ps3scale_game_t *game = selected_game();
    const ps3scale_profile_t *profile = profile_for_game(game);
    int script_state = g_details_cache_valid ? g_details_script_state : 0;
    int can_apply = profile &&
        strcmp(profile->trust, "B_COMMUNITY") == 0 &&
        profile->patch_count == 1;

    draw_page_header("FPS Control",
        "Choose an exact game-version profile; PS3 SCALE never guesses patch addresses.");

    if (!game || !game_is_ps3(game)) {
        draw_soft_card(24.0f, 92.0f, 806.0f, 340.0f, 0);
        draw_text(48.0f, 128.0f, 0x29243fff,
                  "FPS profiles are currently available for PS3 games only.");
        draw_text(48.0f, PS3SCALE_UI_FOOTER_SINGLE_Y, 0x77718fff,
                  "CIRCLE / SELECT / LEFT: BACK");
        set_ui_font_size(14, 22);
        return;
    }

    if (!profile)
        g_fps_control_selection = 0;
    if (g_fps_control_selection < 0 || g_fps_control_selection > 1)
        g_fps_control_selection = 0;

    draw_soft_card(20.0f, 88.0f, 814.0f, 344.0f, 0);
    set_ui_font_size(11, 17);
    snprintf(line, sizeof(line), "%.64s", game->title);
    draw_text(42.0f, 104.0f, 0x29243fff, line);
    set_ui_font_size(7, 11);
    snprintf(line, sizeof(line), "TITLE ID: %.9s     APP VERSION: %.5s",
             game->title_id, game->app_ver[0] ? game->app_ver : "UNKNOWN");
    draw_text(42.0f, 130.0f, 0x514b69ff, line);

    draw_soft_card(38.0f, 158.0f, 374.0f, 88.0f,
                   g_fps_control_selection == 0);
    set_ui_font_size(10, 15);
    draw_text(56.0f, 176.0f, 0x29243fff, "ORIGINAL");
    set_ui_font_size(7, 11);
    draw_text(56.0f, 202.0f, 0x514b69ff,
              "Use the game's native frame-rate behavior.");
    draw_text(56.0f, 222.0f, 0x77718fff,
              "No PS3 SCALE memory patch is armed.");

    draw_soft_card(426.0f, 158.0f, 390.0f, 88.0f,
                   g_fps_control_selection == 1 && profile != NULL);
    if (profile) {
        set_ui_font_size(10, 15);
        snprintf(line, sizeof(line), "%.54s", profile->effect);
        draw_text(444.0f, 176.0f, 0x29243fff, line);
        set_ui_font_size(7, 11);
        draw_text(444.0f, 202.0f, 0x514b69ff,
                  strcmp(profile->trust, "B_COMMUNITY") == 0 ?
                  "COMMUNITY-REPORTED PROFILE" :
                  "MANUAL-ONLY COMMUNITY PROFILE");
        snprintf(line, sizeof(line), "%u memory write(s); exact version match.",
                 profile->patch_count);
        draw_text(444.0f, 222.0f, 0x77718fff, line);
    } else {
        set_ui_font_size(10, 15);
        draw_text(444.0f, 176.0f, 0xb27b35ff,
                  "NO EXACT FPS PROFILE");
        set_ui_font_size(7, 11);
        draw_text(444.0f, 202.0f, 0x514b69ff,
                  "No profile matches this Title ID + APP_VER.");
        draw_text(444.0f, 222.0f, 0x77718fff,
                  "The game remains unmodified; no generic slider exists.");
    }

    draw_soft_card(38.0f, 258.0f, 778.0f, 98.0f, 0);
    set_ui_font_size(8, 12);
    draw_text(56.0f, 274.0f, 0x29243fff, "PATCH STATUS");
    set_ui_font_size(7, 11);
    if (script_state == 1) {
        draw_text(56.0f, 298.0f, 0x26794bff,
                  "ONE-LAUNCH PROFILE ARMED FOR THE NEXT GAME START.");
    } else if (script_state == -2) {
        draw_text(56.0f, 298.0f, 0xc58a4dff,
                  "STALE PS3 SCALE SCRIPT FOUND. REMOVE IT BEFORE RE-ARMING.");
    } else if (script_state == -1) {
        draw_text(56.0f, 298.0f, 0xc58a4dff,
                  "A USER WEBMAN SCRIPT EXISTS. PS3 SCALE WILL NOT CHANGE IT.");
    } else {
        draw_text(56.0f, 298.0f, 0x514b69ff,
                  "NO PROFILE IS ARMED. ORIGINAL GAME BEHAVIOR IS PRESERVED.");
    }
    if (profile && !can_apply) {
        draw_text(56.0f, 322.0f, 0xc58a4dff,
                  "AUTO APPLY DISABLED: MANUAL-ONLY OR MULTI-WRITE PROFILE.");
    } else {
        draw_text(56.0f, 322.0f, 0x77718fff,
                  "Community data is not hardware-verified; 60 FPS is not guaranteed.");
    }

    draw_soft_button(38.0f, 374.0f, 244.0f, 42.0f,
                     "CROSS: PREPARE PROFILE", 0);
    draw_soft_button(294.0f, 374.0f, 244.0f, 42.0f,
                     "TRIANGLE: CONFIRM APPLY",
                     g_profile_confirm_pending);
    draw_soft_button(550.0f, 374.0f, 266.0f, 42.0f,
                     "SQUARE: REMOVE PROFILE", 0);

    set_ui_font_size(7, 11);
    draw_text(24.0f, PS3SCALE_UI_FOOTER_PRIMARY_Y, 0x77718fff,
              g_profile_confirm_pending ?
              "COMMUNITY PATCH: TRIANGLE CONFIRMS; CIRCLE CANCELS." :
              g_details_message);
    draw_text(24.0f, PS3SCALE_UI_FOOTER_SECONDARY_Y, 0x875be8ff,
              "UP/DOWN: SELECT MODE   |   EXIT GAME TO CLEAR MEMORY PATCH.");
    set_ui_font_size(14, 22);
}

static void draw_settings(void)
{
    static const char *labels[SETTINGS_COUNT] = {
        "Music", "Music Volume", "Animations (UI & Background)",
        "Check for USB Update", "Rescan Game Library",
        "Restore Default Settings", "Language", "Music Playlist",
        "Track Fade Duration", "Enhancement Storage"
    };
    char value[96];
    int i;
    int active_group = 2; /* Audio */
    if (g_settings_selected == 2)
        active_group = 1; /* Display */
    else if (g_settings_selected == 3 || g_settings_selected == 4 ||
             g_settings_selected == 9)
        active_group = 3; /* System */
    else if (g_settings_selected == 5 || g_settings_selected == 6)
        active_group = 0; /* General */

    draw_page_header("Settings", "Personalize PS3 SCALE; preferences are saved on the internal HDD");

    /* The selected group follows the focused setting; the chips are not fake tabs. */
    draw_soft_card(20.0f, 88.0f, 148.0f, 354.0f, 0);
    set_ui_font_size(8, 12);
    draw_text(34.0f, 104.0f, 0x29243fff, "PREFERENCES");
    draw_soft_chip(28.0f, 136.0f, 132.0f, "General", active_group == 0);
    draw_soft_chip(28.0f, 184.0f, 132.0f, "Display", active_group == 1);
    draw_soft_chip(28.0f, 232.0f, 132.0f, "Audio", active_group == 2);
    draw_soft_chip(28.0f, 280.0f, 132.0f, "System", active_group == 3);
    set_ui_font_size(6, 9);
    draw_text(34.0f, 348.0f, 0x77718fff, "GROUP AUTO-HIGHLIGHTS");
    draw_text(34.0f, 370.0f, 0x514b69ff, "UP / DOWN TO SELECT");
    draw_text(34.0f, 392.0f, 0x514b69ff, "CROSS TO CHANGE");

    draw_soft_card(184.0f, 88.0f, 650.0f, 354.0f, 0);
    set_ui_font_size(9, 14);
    draw_text(204.0f, 104.0f, 0x29243fff, "All Settings");
    set_ui_font_size(6, 9);
    draw_text(704.0f, 110.0f, 0x77718fff, "CURRENT VALUE");

    for (i = 0; i < SETTINGS_COUNT; i++) {
        float y = 128.0f + (float)i * 27.0f;
        int selected = (i == g_settings_selected);
        if (selected) {
            draw_rounded_rect(198.0f, y - 3.0f, 620.0f, 25.0f,
                              7.0f, 0xeee8ffff);
            draw_rounded_outline(198.0f, y - 3.0f, 620.0f, 25.0f,
                                 7.0f, 0xbba5f0ff);
            draw_solid_rect(202.0f, y + 3.0f, 3.0f, 12.0f, 0x875be8ff);
        } else {
            draw_rounded_rect(198.0f, y - 3.0f, 620.0f, 25.0f,
                              7.0f, 0xf8f6ffb0);
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
        else if (i == 7 && g_music_mode == 0) {
            const char *first = (g_music_start_track == 1) ? "Music 1" : "Music 2";
            const char *second = (g_music_start_track == 1) ? "Music 2" : "Music 1";
            snprintf(value, sizeof(value), "AUTO: %s > %s", first, second);
        } else if (i == 7) {
            snprintf(value, sizeof(value), "%s",
                     g_music_mode == 1 ? "MUSIC 1 ONLY" : "MUSIC 2 ONLY");
        } else if (i == 8) {
            snprintf(value, sizeof(value), "%d MS", g_music_fade_ms);
        } else if (i == 9) {
            snprintf(value, sizeof(value), "%s",
                     g_enhancement_persistent ? "PERMANENT" : "TEMPORARY");
        }

        set_ui_font_size(7, 11);
        draw_text(214.0f, y + 3.0f,
                  selected ? 0x6b43b5ff : 0x29243fff, labels[i]);
        draw_text(604.0f, y + 3.0f,
                  selected ? 0x6b43b5ff : 0x77718fff, value);

        if (i == 0 || i == 2 || i == 9) {
            int enabled = ((i == 0 && g_music_enabled) ||
                           (i == 2 && g_animation_enabled) ||
                           (i == 9 && g_enhancement_persistent));
            draw_rounded_rect(748.0f, y, 54.0f, 18.0f, 9.0f,
                              enabled ? 0xe5d9ffff : 0xe7e3f0ff);
            draw_rounded_outline(748.0f, y, 54.0f, 18.0f, 9.0f,
                                 0xb9a7e8ff);
            draw_rounded_rect(enabled ? 782.0f : 752.0f, y + 3.0f,
                              12.0f, 12.0f, 6.0f, 0x875be8ff);
        } else if (i == 1) {
            float bar = (float)g_music_volume / 255.0f * 112.0f;
            draw_rounded_rect(680.0f, y + 13.0f, 112.0f, 4.0f,
                              2.0f, 0xe0d9f0ff);
            if (bar > 0.0f)
                draw_rounded_rect(680.0f, y + 13.0f, bar, 4.0f,
                                  2.0f, 0x875be8ff);
            draw_rounded_rect(676.0f + bar, y + 9.0f, 12.0f, 12.0f,
                              6.0f, 0x875be8ff);
        }
    }

    set_ui_font_size(6, 9);
    if (g_music_track1_missing)
        draw_text(202.0f, 418.0f, 0xffcc66ff,
                  "MUSIC 1: BIOS - ZORROVIAN (USB IMPORT) | MUSIC 2: GTA V (BUILT IN).");
    else if (g_music1_imported_from_usb)
        draw_text(202.0f, 418.0f, 0x468f78ff,
                  "MUSIC 1 IMPORTED FROM USB AND SAVED TO THE INTERNAL HDD.");
    else
        draw_text(202.0f, 418.0f, 0x77718fff, g_settings_message);

    draw_text(24.0f, PS3SCALE_UI_FOOTER_PRIMARY_Y, 0x77718fff,
              "UP / DOWN: MOVE   |   CROSS: CHANGE   |   LEFT / RIGHT: ADJUST");
    draw_text(24.0f, PS3SCALE_UI_FOOTER_SECONDARY_Y, 0x875be8ff, "CIRCLE: BACK");
    set_ui_font_size(14, 22);
}

static void draw_tools(void)
{
    char line[128];
    int selected_apollo = (g_tools_selected == 0);

    draw_page_header("Companion Tools", "Open installed tools directly from PS3 SCALE");
    draw_soft_card(24.0f, 96.0f, 806.0f, 320.0f, 0);

    draw_rounded_rect(40.0f, 108.0f, 774.0f, 78.0f, 9.0f,
                      selected_apollo ? 0xeee8ffff : 0xf8f6ffb0);
    if (selected_apollo)
        draw_rounded_outline(40.0f, 108.0f, 774.0f, 78.0f, 9.0f,
                             0xbba5f0ff);
    set_ui_font_size(10, 15);
    draw_text(48.0f, 120.0f, 0x29243fff, "APOLLO SAVE TOOL");
    snprintf(line, sizeof(line), "STATUS: %s",
             g_apollo_installed ? "INSTALLED" : "NOT DETECTED");
    draw_text(642.0f, 120.0f,
              g_apollo_installed ? 0x2c9f8eff : 0xffcc66ff, line);
    set_ui_font_size(7, 11);
    draw_text(48.0f, 148.0f, 0x77718fff,
              "Open Apollo for save backup, restore, resign and license tools.");

    draw_rounded_rect(40.0f, 198.0f, 774.0f, 78.0f, 9.0f,
                      selected_apollo ? 0xf8f6ffb0 : 0xeee8ffff);
    if (!selected_apollo)
        draw_rounded_outline(40.0f, 198.0f, 774.0f, 78.0f, 9.0f,
                             0xbba5f0ff);
    set_ui_font_size(10, 15);
    draw_text(48.0f, 210.0f, 0x29243fff, "ARTEMIS PS3");
    snprintf(line, sizeof(line), "STATUS: %s",
             g_artemis_installed ? "INSTALLED" : "NOT DETECTED");
    draw_text(642.0f, 210.0f,
              g_artemis_installed ? 0x2c9f8eff : 0xffcc66ff, line);
    set_ui_font_size(7, 11);
    draw_text(48.0f, 238.0f, 0x77718fff,
              "Open Artemis to select and run a downloaded game NCL code.");

    draw_rounded_rect(40.0f, 288.0f, 774.0f, 66.0f, 9.0f, 0xf8f6ffb0);
    set_ui_font_size(9, 13);
    draw_text(48.0f, 300.0f, 0x29243fff, "webMAN MOD");
    snprintf(line, sizeof(line), "FILE HINT: %s",
             g_webman_installed ? "FOUND" : "NOT DETECTED");
    draw_text(642.0f, 300.0f,
              g_webman_installed ? 0x2c9f8eff : 0xffcc66ff, line);
    set_ui_font_size(7, 11);
    draw_text(48.0f, 324.0f, 0x77718fff,
              "Local launch bridge; it must be active to open companion apps.");

    draw_text(48.0f, 382.0f,
              g_tool_launch_running ? 0x875be8ff : 0x77718fff,
              g_tools_message);
    draw_text(24.0f, PS3SCALE_UI_FOOTER_PRIMARY_Y, 0x77718fff,
              "UP / DOWN: SELECT   |   CROSS: OPEN   |   TRIANGLE: REFRESH");
    draw_text(24.0f, PS3SCALE_UI_FOOTER_SECONDARY_Y, 0x875be8ff,
              "CIRCLE: BACK");
    set_ui_font_size(14, 22);
}

static void draw_help(void)
{
    static const char *step_titles[5] = {
        "Scan Games", "Select a Game", "Review Match",
        "Save Data Safely", "Start Game"
    };
    static const char *step_lines[5] = {
        "Find PS3 / PS2 titles.",
        "Choose a local title.",
        "Check ID and version.",
        "Save NCL / TXT data.",
        "Launch when ready."
    };
    static const char *actions[3] = {
        "Report an Issue", "Send Feedback", "Retry Pending"
    };
    int i;

    draw_page_header("Help & Support", "A quick guide to the main PS3 SCALE features");
    set_ui_font_size(8, 12);
    draw_text(24.0f, 88.0f, 0x77718fff,
              "GETTING STARTED  /  FIVE SIMPLE STEPS");

    for (i = 0; i < 5; i++) {
        float x = 20.0f + (float)i * 164.0f;
        draw_soft_card(x, 112.0f, 154.0f, 202.0f, 0);
        draw_rounded_rect(x + 12.0f, 126.0f, 28.0f, 24.0f,
                          8.0f, 0xeee7ffff);
        draw_rounded_outline(x + 12.0f, 126.0f, 28.0f, 24.0f,
                             8.0f, 0xb99affff);
        set_ui_font_size(8, 12);
        {
            char number[8];
            snprintf(number, sizeof(number), "%d", i + 1);
            draw_text(x + 22.0f, 132.0f, 0x875be8ff, number);
        }
        draw_menu_icon(i, x + 96.0f, 124.0f, 0);
        set_ui_font_size(8, 12);
        draw_text(x + 12.0f, 176.0f, 0x29243fff, step_titles[i]);
        set_ui_font_size(6, 9);
        draw_text(x + 12.0f, 207.0f, 0x514b69ff, step_lines[i]);
        draw_rounded_rect(x + 12.0f, 270.0f, 130.0f, 3.0f,
                          1.5f, 0xe4ddefee);
        draw_rounded_rect(x + 12.0f, 270.0f,
                          (float)(i + 1) * 26.0f, 3.0f,
                          1.5f, 0x9568edff);
    }

    draw_soft_card(20.0f, 332.0f, 814.0f, 112.0f, 0);
    set_ui_font_size(9, 14);
    draw_text(38.0f, 346.0f, 0x29243fff, "Support & Contact");
    set_ui_font_size(7, 11);
    draw_text(38.0f, 376.0f, 0x514b69ff, "support@ps3scale.app");
    draw_soft_button(242.0f, 362.0f, 168.0f, 42.0f,
                     actions[0], g_help_selected == 0);
    draw_soft_button(420.0f, 362.0f, 168.0f, 42.0f,
                     actions[1], g_help_selected == 1);
    draw_soft_button(598.0f, 362.0f, 168.0f, 42.0f,
                     actions[2], g_help_selected == 2);
    set_ui_font_size(6, 9);
    draw_text(38.0f, 414.0f, 0x77718fff,
              "Community files are saved as data only; they are never auto-executed.");
    draw_text(38.0f, 430.0f,
              g_support_send_success ? 0x2c9f8eff : 0x77718fff,
              g_support_status);
    set_ui_font_size(7, 11);
    draw_text(24.0f, PS3SCALE_UI_FOOTER_SINGLE_Y, 0x77718fff,
              "UP / DOWN: CHOOSE SUPPORT ACTION   |   CROSS: OPEN   |   CIRCLE: BACK");
    set_ui_font_size(14, 22);
}

static void draw_support_form(void)
{
    int i;
    char title_preview[96];
    char details_preview[96];
    static const char *labels[3] = {
        "Report Title", "Problem Details", "Submit Report"
    };

    copy_text_tail_preview(title_preview, sizeof(title_preview), g_support_title);
    copy_text_tail_preview(details_preview, sizeof(details_preview), g_support_details);
    draw_page_header(g_support_is_feedback ? "Send Feedback" : "Report an Issue",
                     "Send a note from PS3 SCALE");
    draw_soft_card(24.0f, 96.0f, 806.0f, 330.0f, 0);
    for (i = 0; i < 3; i++) {
        float y = 118.0f + (float)i * 86.0f;
        int selected = (i == g_support_selected);
        draw_soft_card(42.0f, y, 768.0f, 68.0f, selected);
        set_ui_font_size(8, 12);
        draw_text(60.0f, y + 9.0f,
                  selected ? 0x875be8ff : 0x514b69ff, labels[i]);
        set_ui_font_size(7, 11);
        if (i == 0)
            draw_text(60.0f, y + 38.0f,
                      g_support_title[0] ? 0x29243fff : 0x77718fff,
                      g_support_title[0] ? title_preview :
                                           "Press CROSS to enter a title");
        else if (i == 1)
            draw_text(60.0f, y + 38.0f,
                      g_support_details[0] ? 0x29243fff : 0x77718fff,
                      g_support_details[0] ? details_preview :
                                             "Press CROSS to describe the issue");
        else
            draw_text(60.0f, y + 38.0f, 0x77718fff,
                      "Save locally; send online if the service is configured");
    }
    set_ui_font_size(7, 11);
    draw_text(44.0f, 388.0f, 0x77718fff,
              "Model, firmware, platform and app version are added automatically.");
    draw_text(44.0f, 400.0f, 0xa35b34ff,
              "Never include PSN IDs, passwords or account tokens.");
    draw_text(44.0f, 414.0f, 0x77718fff, g_support_status);
    draw_text(24.0f, PS3SCALE_UI_FOOTER_SINGLE_Y, 0x77718fff,
              "UP / DOWN: MOVE   |   CROSS: EDIT / SAVE   |   CIRCLE: BACK");
    set_ui_font_size(14, 22);
}

static void generate_support_request_id(void)
{
    struct timeval now;
    unsigned long seconds;
    unsigned long micros;
    unsigned long process_id;
    unsigned long sequence;
    unsigned long long tail;

    if (gettimeofday(&now, NULL) != 0) {
        now.tv_sec = time(NULL);
        now.tv_usec = (suseconds_t)(++g_support_request_sequence & 0xffffu);
    }
    seconds = (unsigned long)now.tv_sec;
    micros = (unsigned long)now.tv_usec;
    process_id = (unsigned long)getpid();
    sequence = ++g_support_request_sequence;
    tail = ((unsigned long long)seconds << 20) ^
           ((unsigned long long)micros << 4) ^
           ((unsigned long long)process_id << 12) ^
           (unsigned long long)sequence;

    (void)snprintf(g_support_request_id, sizeof(g_support_request_id),
                   "%08lx-%04lx-4%03lx-a%03lx-%012llx",
                   seconds & 0xfffffffful,
                   micros & 0xfffful,
                   process_id & 0xffful,
                   sequence & 0xffful,
                   tail & 0xffffffffffffull);
}

static int build_support_json_to(char *destination, size_t capacity)
{
    char model[81];
    char firmware[41];
    char platform[81];

    if (g_hardware_info_valid) {
        (void)snprintf(model, sizeof(model), "%s",
                       g_hardware_info.model_series);
        (void)snprintf(firmware, sizeof(firmware), "%s",
                       g_hardware_info.firmware_version);
        (void)snprintf(platform, sizeof(platform), "%s",
                       g_hardware_info.platform_id);
    } else {
        (void)snprintf(model, sizeof(model), "UNKNOWN");
        (void)snprintf(firmware, sizeof(firmware), "UNKNOWN");
        (void)snprintf(platform, sizeof(platform), "UNKNOWN");
    }

    return ps3scale_support_build_json(
        destination, capacity, g_support_request_id,
        g_support_is_feedback ? "feedback" : "issue",
        g_support_title, g_support_details, model, firmware, platform,
        PS3SCALE_APP_VERSION);
}

static int build_support_json(void)
{
    return build_support_json_to(g_support_json, sizeof(g_support_json));
}

static void support_submit_worker(void *arg)
{
    long response_code = 0;
    (void)arg;
    g_support_send_result = ps3scale_online_post_json(
        PS3SCALE_SUPPORT_URL, PS3SCALE_SUPPORT_API_KEY,
        g_support_json, strlen(g_support_json), &response_code);
    g_support_send_http_status = response_code;
    g_support_send_done = 1;
    g_support_send_running = 0;
    g_network_task_running = 0;
}

static void start_support_send(void)
{
    int rc;

    if (!g_support_report_saved || !g_support_request_id[0]) {
        snprintf(g_support_status, sizeof(g_support_status),
                 "SAVE REPORT LOCALLY FIRST.");
        return;
    }
    if (g_support_send_success) {
        snprintf(g_support_status, sizeof(g_support_status),
                 "REPORT ALREADY SENT ONLINE.");
        return;
    }
    if (g_support_thread_started || g_support_send_running) {
        snprintf(g_support_status, sizeof(g_support_status),
                 "ANOTHER REPORT IS SENDING.");
        return;
    }
    if (strstr(PS3SCALE_SUPPORT_URL, "YOUR_PROJECT") != NULL ||
        strstr(PS3SCALE_SUPPORT_API_KEY, "YOUR_PUBLISHABLE_KEY") != NULL) {
        snprintf(g_support_status, sizeof(g_support_status),
                 "LOCAL SAVE OK; ONLINE OFF.");
        return;
    }
    if (g_network_task_running ||
        g_catalog_sync_running || g_catalog_thread_started ||
        g_game_launch_running || g_game_launch_thread_started ||
        g_profile_arm_running || g_profile_arm_thread_started) {
        snprintf(g_support_status, sizeof(g_support_status),
                 "LOCAL SAVE OK; NETWORK BUSY. PRESS CROSS TO RETRY.");
        return;
    }
    if (!g_support_json_ready) {
        if (build_support_json() != 0) {
            snprintf(g_support_status, sizeof(g_support_status),
                     "LOCAL SAVE OK; REPORT FORMAT INVALID.");
            return;
        }
        g_support_json_ready = 1;
    }

    (void)snprintf(g_support_sending_request_id,
                   sizeof(g_support_sending_request_id), "%s",
                   g_support_request_id);
    (void)snprintf(g_support_sending_path, sizeof(g_support_sending_path),
                   "%s", g_support_pending_path);
    g_support_send_done = 0;
    g_support_send_result = -1;
    g_support_send_http_status = 0;
    g_support_send_running = 1;
    g_network_task_running = 1;
    rc = sysThreadCreate(&g_support_thread, support_submit_worker, NULL,
                         1400, 0x8000, THREAD_JOINABLE,
                         "PS3 SCALE Support");
    if (rc == 0) {
        g_support_thread_started = 1;
        snprintf(g_support_status, sizeof(g_support_status),
                 "REPORT QUEUED FOR ONLINE SEND.");
    } else {
        g_support_send_running = 0;
        g_network_task_running = 0;
        snprintf(g_support_status, sizeof(g_support_status),
                 "LOCAL SAVE OK; SEND START FAILED. PRESS CROSS TO RETRY.");
    }
}

static void service_support_send(void)
{
    int is_current_report;

    if (!g_support_send_done)
        return;
    if (g_support_thread_started) {
        u64 thread_result = 0;
        (void)sysThreadJoin(g_support_thread, &thread_result);
        g_support_thread_started = 0;
    }
    is_current_report =
        strcmp(g_support_sending_request_id, g_support_request_id) == 0;
    if (g_support_send_result == 0) {
        if (g_support_sending_path[0])
            (void)unlink(g_support_sending_path);
        if (is_current_report) {
            g_support_send_success = 1;
            g_support_json_ready = 0;
            g_support_pending_path[0] = '\0';
            snprintf(g_support_status, sizeof(g_support_status),
                     "REPORT SENT ONLINE.");
        } else if (g_screen == SCREEN_HELP) {
            snprintf(g_support_status, sizeof(g_support_status),
                     "PENDING REPORT SENT ONLINE.");
        }
    } else if (is_current_report && g_support_send_http_status == 429) {
        snprintf(g_support_status, sizeof(g_support_status),
                 "SERVER LIMIT REACHED; TRY TOMORROW.");
    } else if (is_current_report && g_support_send_http_status >= 500) {
        snprintf(g_support_status, sizeof(g_support_status),
                 "SERVER TEMPORARILY UNAVAILABLE; RETRY.");
    } else if (is_current_report && g_support_send_http_status >= 400) {
        snprintf(g_support_status, sizeof(g_support_status),
                 "SERVER REJECTED REPORT; CHECK DETAILS.");
    } else if (is_current_report) {
        snprintf(g_support_status, sizeof(g_support_status),
                 "LOCAL SAVE OK; SEND FAILED. PRESS CROSS TO RETRY.");
    } else if (g_screen == SCREEN_HELP) {
        snprintf(g_support_status, sizeof(g_support_status),
                 "PENDING REPORT FAILED; TRY AGAIN LATER.");
    }
    g_support_sending_path[0] = '\0';
    g_support_send_done = 0;
}

static int retry_pending_support_report(void)
{
    DIR *directory;
    struct dirent *entry;
    char selected_name[64] = "";
    char report_path[512];
    char request_id[PS3SCALE_SUPPORT_REQUEST_ID_LENGTH + 1];
    FILE *file;
    size_t bytes;
    int path_length;
    const char *marker;

    if (g_support_thread_started || g_support_send_running ||
        g_network_task_running || g_catalog_sync_running ||
        g_catalog_thread_started || g_game_launch_running ||
        g_game_launch_thread_started || g_profile_arm_running ||
        g_profile_arm_thread_started) {
        snprintf(g_support_status, sizeof(g_support_status),
                 "NETWORK BUSY; TRY PENDING REPORTS AGAIN.");
        return -1;
    }

    directory = opendir(PS3SCALE_SUPPORT_OUTBOX_DIR);
    if (!directory) {
        snprintf(g_support_status, sizeof(g_support_status),
                 "NO PENDING REPORTS.");
        return -1;
    }
    while ((entry = readdir(directory)) != NULL) {
        size_t name_length = strlen(entry->d_name);
        if (name_length != PS3SCALE_SUPPORT_REQUEST_ID_LENGTH + 5u ||
            strcmp(entry->d_name + PS3SCALE_SUPPORT_REQUEST_ID_LENGTH,
                   ".json") != 0)
            continue;
        memcpy(request_id, entry->d_name,
               PS3SCALE_SUPPORT_REQUEST_ID_LENGTH);
        request_id[PS3SCALE_SUPPORT_REQUEST_ID_LENGTH] = '\0';
        if (!ps3scale_support_request_id_valid(request_id))
            continue;
        if (!selected_name[0] || strcmp(entry->d_name, selected_name) < 0)
            memcpy(selected_name, entry->d_name, name_length + 1);
    }
    (void)closedir(directory);

    if (!selected_name[0]) {
        snprintf(g_support_status, sizeof(g_support_status),
                 "NO PENDING REPORTS.");
        return -1;
    }

    memcpy(request_id, selected_name, PS3SCALE_SUPPORT_REQUEST_ID_LENGTH);
    request_id[PS3SCALE_SUPPORT_REQUEST_ID_LENGTH] = '\0';
    path_length = snprintf(report_path, sizeof(report_path), "%s/%s",
                           PS3SCALE_SUPPORT_OUTBOX_DIR, selected_name);
    if (path_length < 0 || (size_t)path_length >= sizeof(report_path)) {
        snprintf(g_support_status, sizeof(g_support_status),
                 "PENDING REPORT PATH INVALID.");
        return -1;
    }

    file = fopen(report_path, "rb");
    if (!file) {
        snprintf(g_support_status, sizeof(g_support_status),
                 "PENDING REPORT COULD NOT BE OPENED.");
        return -1;
    }
    bytes = fread(g_support_json, 1, sizeof(g_support_json) - 1, file);
    {
        int read_error = ferror(file);
        int has_extra_data = fgetc(file) != EOF;
        int close_error = fclose(file) != 0;
        if (read_error || has_extra_data || close_error) {
            snprintf(g_support_status, sizeof(g_support_status),
                     "PENDING REPORT IS TOO LARGE OR DAMAGED.");
            return -1;
        }
    }
    g_support_json[bytes] = '\0';
    marker = strstr(g_support_json, "\"request_id\":\"");
    if (!marker) {
        snprintf(g_support_status, sizeof(g_support_status),
                 "PENDING REPORT IS NOT VALID JSON.");
        return -1;
    }
    marker += strlen("\"request_id\":\"");
    if (strlen(marker) < PS3SCALE_SUPPORT_REQUEST_ID_LENGTH + 1u ||
        strncmp(marker, request_id, PS3SCALE_SUPPORT_REQUEST_ID_LENGTH) != 0 ||
        marker[PS3SCALE_SUPPORT_REQUEST_ID_LENGTH] != '"') {
        snprintf(g_support_status, sizeof(g_support_status),
                 "PENDING REPORT ID DOES NOT MATCH FILE.");
        return -1;
    }

    (void)snprintf(g_support_request_id, sizeof(g_support_request_id), "%s",
                   request_id);
    (void)snprintf(g_support_pending_path, sizeof(g_support_pending_path), "%s",
                   report_path);
    g_support_report_saved = 1;
    g_support_send_success = 0;
    g_support_json_ready = 1;
    start_support_send();
    return 0;
}

static int save_support_report(void)
{
    char validation_json[8192];
    int path_length;

    if (g_support_report_saved) {
        if (g_support_send_success) {
            snprintf(g_support_status, sizeof(g_support_status),
                     "REPORT ALREADY SENT ONLINE.");
            return 0;
        }
        start_support_send();
        return 0;
    }
    if (!g_support_title[0] || !g_support_details[0]) {
        snprintf(g_support_status, sizeof(g_support_status),
                 "ADD TITLE + DETAILS.");
        return -1;
    }

    generate_support_request_id();
    if (build_support_json_to(validation_json, sizeof(validation_json)) != 0) {
        g_support_request_id[0] = '\0';
        snprintf(g_support_status, sizeof(g_support_status),
                 "REPORT TOO LONG OR INVALID.");
        return -1;
    }

    path_length = snprintf(g_support_pending_path,
                           sizeof(g_support_pending_path), "%s/%s.json",
                           PS3SCALE_SUPPORT_OUTBOX_DIR,
                           g_support_request_id);
    if ((mkdir(PS3SCALE_SUPPORT_DIR, 0777) != 0 &&
         access(PS3SCALE_SUPPORT_DIR, F_OK) != 0) ||
        (mkdir(PS3SCALE_SUPPORT_OUTBOX_DIR, 0777) != 0 &&
         access(PS3SCALE_SUPPORT_OUTBOX_DIR, F_OK) != 0) ||
        path_length < 0 ||
        (size_t)path_length >= sizeof(g_support_pending_path) ||
        ps3scale_atomic_file_write(g_support_pending_path, validation_json,
                                   strlen(validation_json)) != 0) {
        g_support_request_id[0] = '\0';
        g_support_pending_path[0] = '\0';
        snprintf(g_support_status, sizeof(g_support_status),
                 "REPORT SAVE FAILED; PREVIOUS REPORTS KEPT.");
        return -1;
    }

    g_support_report_saved = 1;
    g_support_json_ready = 0;
    snprintf(g_support_status, sizeof(g_support_status),
             "SAVED LOCALLY; SENDING IF ONLINE.");
    start_support_send();
    return 0;
}

static void draw_about(void)
{
    char version_line[128];

    if (g_hardware_info_valid) {
        snprintf(version_line, sizeof(version_line),
                 "%s | FW %s | PS3 %s | %s",
                 PS3SCALE_APP_VERSION, g_hardware_info.firmware_version,
                 ps3scale_hardware_family_name(g_hardware_info.family),
                 ps3scale_console_type_name(g_hardware_info.console_type));
    } else {
        snprintf(version_line, sizeof(version_line),
                 "%s | HARDWARE INFORMATION UNAVAILABLE",
                 PS3SCALE_APP_VERSION);
    }

    draw_page_header("About", "Credits, hardware and safety information");
    draw_soft_card(20.0f, 88.0f, 814.0f, 356.0f, 0);

    set_ui_font_size(14, 21);
    draw_text(42.0f, 108.0f, 0x29243fff, "PS3 SCALE");
    set_ui_font_size(7, 11);
    draw_text(42.0f, 142.0f, 0x514b69ff,
              "PS3 / PS2 Game Library & Enhancement Manager");
    draw_text(42.0f, 166.0f, 0x77718fff,
              "Organize local games and review version-matched community profiles.");
    draw_text(42.0f, 190.0f, 0x77718fff,
              "Scanning is read-only. Original game files are never replaced.");
    draw_text(42.0f, 214.0f, 0x77718fff,
              "No universal upscaler or guaranteed 60 FPS is claimed.");

    draw_soft_chip(42.0f, 238.0f, 154.0f, "READ-ONLY SCANNING", 1);
    draw_soft_chip(204.0f, 238.0f, 176.0f, "VERSION-MATCHED DATA", 0);
    draw_soft_chip(388.0f, 238.0f, 190.0f, "EXACT-VERSION AUTO APPLY", 0);

    set_ui_font_size(9, 14);
    draw_text(42.0f, 282.0f, 0x29243fff, "Hardware Information");
    draw_rounded_rect(394.0f, 276.0f, 184.0f, 23.0f, 7.0f,
                      g_hardware_info_valid ? 0xe5f5eaff : 0xfff3e5ff);
    set_ui_font_size(6, 9);
    draw_text(404.0f, 283.0f,
              g_hardware_info_valid ? 0x26794bff : 0xb27b35ff,
              g_hardware_info_valid ? "CONSOLE DETECTED" : "NOT AVAILABLE");

    if (g_hardware_info_valid) {
        set_ui_font_size(7, 11);
        draw_text(42.0f, 316.0f, 0x77718fff, "Model");
        draw_text(176.0f, 316.0f, 0x29243fff, g_hardware_info.model_series);
        draw_text(42.0f, 340.0f, 0x77718fff, "Console type");
        draw_text(176.0f, 340.0f, 0x29243fff,
                  ps3scale_console_type_name(g_hardware_info.console_type));
        draw_text(42.0f, 364.0f, 0x77718fff, "Firmware");
        draw_text(176.0f, 364.0f, 0x29243fff,
                  g_hardware_info.firmware_version);
        draw_text(42.0f, 388.0f, 0x77718fff, "Platform ID");
        draw_text(176.0f, 388.0f, 0x29243fff,
                  g_hardware_info.platform_id);
    } else {
        draw_text(42.0f, 316.0f, 0xb27b35ff,
                  "Hardware information is unavailable.");
        draw_text(42.0f, 340.0f, 0x77718fff,
                  "The application can still browse local information.");
    }
    set_ui_font_size(6, 9);
    draw_text(42.0f, 420.0f, 0x77718fff, version_line);

    set_ui_font_size(7, 11);
    draw_text(24.0f, PS3SCALE_UI_FOOTER_SINGLE_Y, 0x875be8ff,
              "CIRCLE: BACK");
    set_ui_font_size(14, 22);
}

static void draw_profile_toast(void)
{
    if (!strstr(g_details_message, "ONE-LAUNCH PROFILE ARMED"))
        return;
    draw_rounded_rect(568.0f, 16.0f, 264.0f, 48.0f, 10.0f,
                      0xf4effff4);
    draw_rounded_outline(568.0f, 16.0f, 264.0f, 48.0f, 10.0f,
                         0xb99affff);
    draw_rounded_rect(568.0f, 16.0f, 4.0f, 48.0f, 2.0f, 0x875be8ff);
    draw_circle_outline(590.0f, 40.0f, 10.0f, 0x2c9f8eff);
    draw_icon_line(585.0f, 40.0f, 589.0f, 44.0f, 0x2c9f8eff);
    draw_icon_line(589.0f, 44.0f, 596.0f, 35.0f, 0x2c9f8eff);
    set_ui_font_size(8, 12);
    draw_text(610.0f, 28.0f, 0x29243fff, "Profile Ready");
    set_ui_font_size(6, 9);
    draw_text(610.0f, 44.0f, 0x77718fff, "Start the game to use it");
    set_ui_font_size(14, 22);
}

static void draw_screen_transition(void)
{
    unsigned int frame;
    unsigned int alpha;
    float sweep_x;

    if (!g_animation_enabled) {
        g_transition_frames = PS3SCALE_TRANSITION_FRAMES;
        return;
    }
    if (g_transition_frames >= PS3SCALE_TRANSITION_FRAMES)
        return;

    frame = g_transition_frames;
    alpha = ((PS3SCALE_TRANSITION_FRAMES - frame) * 80u) /
            PS3SCALE_TRANSITION_FRAMES;
    sweep_x = 854.0f * (float)frame /
              (float)(PS3SCALE_TRANSITION_FRAMES - 1u);

    /* A brief pearl veil and lavender sweep soften every screen change. */
    draw_solid_rect(0.0f, 0.0f, 854.0f, 512.0f,
                    color_with_alpha(0xfaf8ffff, alpha));
    draw_solid_rect(sweep_x - 10.0f, 0.0f, 20.0f, 512.0f,
                    color_with_alpha(0xb995ffff, 28u));
    draw_solid_rect(sweep_x, 0.0f, 2.0f, 512.0f,
                    color_with_alpha(0x875be8ff, 132u));

    g_transition_frames++;
}

static void draw_ui(void)
{
    if (g_screen != g_transition_screen) {
        g_transition_screen = g_screen;
        g_transition_frames = 0;
    }

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

    /* The animated perspective staircase is projected into the 2D UI pass,
       so it remains visible on every RSX depth-buffer configuration. */
    if (g_animation_enabled)
        draw_home_ambient();

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
        case SCREEN_FPS_CONTROL: draw_fps_control(); break;
        case SCREEN_PS2_CONFIG: draw_ps2_config_manager(); break;
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
    draw_screen_transition();
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
    rebuild_recent_indices();
    clear_scan_cover_cache();
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
    {
        size_t maximum = sizeof(g_search) - 1;
        if (g_keyboard_support_mode)
            maximum = g_support_selected == 0 ?
                PS3SCALE_SUPPORT_TITLE_MAX : PS3SCALE_SUPPORT_DETAILS_MAX;
        if (used + amount <= maximum && used + amount < sizeof(g_search)) {
            memcpy(g_search + used, value, amount + 1);
            g_selected = 0;
            clamp_selection();
        }
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
        if (g_support_selected == 0) {
            if (strcmp(g_support_title, g_search) != 0) {
                snprintf(g_support_title, sizeof(g_support_title), "%s", g_search);
                g_support_report_saved = 0;
                g_support_send_success = 0;
                g_support_json_ready = 0;
                g_support_pending_path[0] = '\0';
                g_support_request_id[0] = '\0';
            }
        } else if (strcmp(g_support_details, g_search) != 0) {
            snprintf(g_support_details, sizeof(g_support_details), "%s", g_search);
            g_support_report_saved = 0;
            g_support_send_success = 0;
            g_support_json_ready = 0;
            g_support_pending_path[0] = '\0';
            g_support_request_id[0] = '\0';
        }
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
    cleanup_temporary_enhancements();
    (void)mkdir(PS3SCALE_PREVIEW_DIR, 0777);
    load_favorites();
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
    if (!g_font_texture) {
        /* ya2d has already initialized the video / pad layer. Release its
         * texture and backend before returning from a failed startup. */
        if (g_apollo_texture) {
            ya2d_freeTexture(g_apollo_texture);
            g_apollo_texture = NULL;
        }
        ya2d_deinit();
        return 1;
    }

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
    /* A partial install must not be reported as healthy. Both the
     * executable and package metadata are required for a valid app layout. */
    g_app_files_ok =
        file_exists_readonly("/dev_hdd0/game/PS3SCALE1/USRDIR/EBOOT.BIN") &&
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
        app_screen_t screen_at_frame_start = g_screen;
        u16 up = 0, down = 0, square = 0, cross = 0, circle = 0;
        u16 l1 = 0, r1 = 0, triangle = 0, select = 0;
        u16 left = 0, right = 0, start = 0;

        sysUtilCheckCallback();
        service_catalog_sync();
        service_support_send();
        service_game_launch();
        service_companion_tool_launch();
        service_profile_arm();
        service_ps2_config_download();
        service_community_download();
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
                g_tools_selected = 0;
                refresh_companion_tool_status();
            }
            if (cross && !old_cross) {
                if (g_home_selected == 0) {
                    g_screen = SCREEN_SCAN;
                    g_selected = 0;
                    g_scan_focus_sidebar = 0;
                    g_scan_sidebar_selected = g_library;
                } else if (g_home_selected == 1) {
                    g_screen = SCREEN_ONLINE_DB;
                    refresh_companion_tool_status();
                    g_online_selected = 0;
                    rebuild_online_results();
                    if (g_community_ready) {
                        snprintf(g_online_message, sizeof(g_online_message),
                                 "SEARCHED %u PS3 GAMES; FOUND %u GITHUB FILES.",
                                 (unsigned)g_online_games_searched,
                                 (unsigned)g_online_community_result_count);
                    } else {
                        snprintf(g_online_message, sizeof(g_online_message),
                                 "OPENING ONLINE DATABASE. REFRESHING ON FIRST OPEN.");
                    }
                    if (!g_online_first_opened) {
                        g_online_first_opened = 1;
                        start_catalog_sync_worker(1);
                    } else if (catalog_cache_is_stale() ||
                               community_index_cache_is_stale()) {
                        start_catalog_sync_worker(0);
                    }
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
            if (up && !old_up)
                g_help_selected = (g_help_selected + 2) % 3;
            if (down && !old_down)
                g_help_selected = (g_help_selected + 1) % 3;
            if (cross && !old_cross) {
                if (g_help_selected == 2) {
                    (void)retry_pending_support_report();
                } else {
                    g_support_is_feedback = (g_help_selected == 1);
                    g_support_selected = 0;
                    g_support_title[0] = '\0';
                    g_support_details[0] = '\0';
                    g_support_request_id[0] = '\0';
                    g_support_pending_path[0] = '\0';
                    g_support_report_saved = 0;
                    g_support_send_success = 0;
                    g_support_json_ready = 0;
                    snprintf(g_support_status, sizeof(g_support_status),
                             "ENTER TITLE + DETAILS.");
                    g_screen = SCREEN_SUPPORT_FORM;
                }
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
            size_t count = library_count(g_library);
            if (l1 && r1 && (!old_l1 || !old_r1)) {
                g_library = LIBRARY_ALL;
                g_scan_sidebar_selected = LIBRARY_ALL;
                g_selected = 0;
                g_scan_focus_sidebar = 0;
            } else if (l1 && !old_l1) {
                g_library = LIBRARY_PS3;
                g_scan_sidebar_selected = LIBRARY_PS3;
                g_selected = 0;
                g_config_preview = 0;
                g_scan_focus_sidebar = 0;
            } else if (r1 && !old_r1) {
                g_library = LIBRARY_PS2;
                g_scan_sidebar_selected = LIBRARY_PS2;
                g_selected = 0;
                g_config_preview = 0;
                g_scan_focus_sidebar = 0;
            }
            if (g_scan_focus_sidebar) {
                if (up && !old_up)
                    g_scan_sidebar_selected =
                        (g_scan_sidebar_selected + 4) % 5;
                if (down && !old_down)
                    g_scan_sidebar_selected =
                        (g_scan_sidebar_selected + 1) % 5;
                if ((right && !old_right) || (cross && !old_cross)) {
                    g_library = g_scan_sidebar_selected;
                    g_selected = 0;
                    g_scan_focus_sidebar = 0;
                    clamp_selection();
                }
                if (left && !old_left)
                    g_scan_focus_sidebar = 0;
            } else {
                if (left && !old_left) {
                    if ((g_selected % 4) == 0) {
                        g_scan_focus_sidebar = 1;
                        g_scan_sidebar_selected = g_library;
                    } else if (g_selected > 0) {
                        g_selected--;
                    }
                }
                if (right && !old_right && (size_t)g_selected + 1 < count)
                    g_selected++;
                if (up && !old_up && g_selected >= 4)
                    g_selected -= 4;
                if (down && !old_down && (size_t)g_selected + 4 < count)
                    g_selected += 4;
                if (cross && !old_cross && selected_game()) {
                    const ps3scale_profile_t *profile =
                        profile_for_game(selected_game());
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
                if (square && !old_square && !select && !start) {
                    toggle_favorite(selected_game());
                    clamp_selection();
                }
            }
            if ((select && !old_select) || (start && !old_start)) {
                if (square)
                    scan_library();
                else
                    (void)open_search_keyboard();
            }
            if (triangle && !old_triangle && g_library == LIBRARY_PS2)
                g_config_preview = !g_config_preview;
            if (circle && !old_circle)
                g_screen = SCREEN_HOME;
        } else if (g_screen == SCREEN_DETAILS) {
            const ps3scale_game_t *game = selected_game();
            const ps3scale_profile_t *profile = profile_for_game(game);
            int back_pressed = (circle && !old_circle) ||
                               (select && !old_select) ||
                               (left && !old_left);

            /* Handle Back first so no stale Cross / Triangle input can launch
               or arm an action while the user is trying to leave this screen. */
            if (back_pressed) {
                if (g_profile_confirm_pending) {
                    g_profile_confirm_pending = 0;
                    set_details_message("PROFILE INSTALL CANCELLED.");
                } else {
                    g_screen = SCREEN_SCAN;
                }
            } else {
                if (square && !old_square && game) {
                    if (game_is_ps3(game)) {
                        g_profile_confirm_pending = 0;
                        g_fps_control_selection = 0;
                        g_screen = SCREEN_FPS_CONTROL;
                    } else if (game->kind == PS3SCALE_GAME_PS2_ISO ||
                               game->kind == PS3SCALE_GAME_PS2_CLASSIC) {
                        g_ps2_config_manager_selection = 0;
                        g_ps2_enhancer_guide_open = 0;
                        g_ps2_enhancement_flags = PS3SCALE_PS2_CFG_ENABLE_L2H;
                        snprintf(g_ps2_config_manager_message,sizeof(g_ps2_config_manager_message),
                                 "L2H IS ON BY DEFAULT. SELECT OPTIONS THEN APPLY.");
                        refresh_ps2_config_manager_cache();
                        refresh_ps2_patch_availability();
                        g_screen = SCREEN_PS2_CONFIG;
                    }
                }
                if (triangle && !old_triangle) {
                    if (g_profile_confirm_pending) {
                        confirm_profile_install();
                    } else if (game && game_is_ps3(game)) {
                        if (!profile) {
                            set_details_message("NO EXACT FPS PROFILE FOR THIS GAME UPDATE.");
                        } else if (strcmp(profile->trust, "B_COMMUNITY_MANUAL") == 0 ||
                                   profile->patch_count != 1) {
                            set_details_message("PROFILE IS MANUAL-ONLY OR MULTI-WRITE. OPEN FPS CONTROL.");
                        } else {
                            g_profile_confirm_pending = 1;
                            set_details_message("COMMUNITY PATCH. TRIANGLE AGAIN TO CONFIRM APPLY.");
                        }
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
            }
        } else if (g_screen == SCREEN_PS2_CONFIG) {
            if (g_ps2_enhancer_guide_open) {
                if ((triangle && !old_triangle) ||
                    (circle && !old_circle) || (select && !old_select))
                    g_ps2_enhancer_guide_open = 0;
            } else if (triangle && !old_triangle) {
                if (g_ps2_patch_confirm_pending) {
                    g_ps2_patch_confirm_pending = 0;
                    snprintf(g_ps2_config_manager_message,
                             sizeof(g_ps2_config_manager_message),
                             "PATCH CONFIRMATION CANCELLED. NO FILE WAS CHANGED.");
                }
                g_ps2_enhancer_guide_open = 1;
            } else if ((circle && !old_circle) ||
                       (select && !old_select) || (left && !old_left)) {
                if (g_ps2_patch_confirm_pending) {
                    g_ps2_patch_confirm_pending = 0;
                    snprintf(g_ps2_config_manager_message,
                             sizeof(g_ps2_config_manager_message),
                             "PATCH CONFIRMATION CANCELLED. NO FILE WAS CHANGED.");
                } else {
                    g_screen = SCREEN_DETAILS;
                    g_details_cache_valid = 0;
                }
            } else {
                if (up && !old_up) {
                    if (g_ps2_patch_confirm_pending) {
                        g_ps2_patch_confirm_pending = 0;
                        snprintf(g_ps2_config_manager_message,
                                 sizeof(g_ps2_config_manager_message),
                                 "PATCH CONFIRMATION CANCELLED. NO FILE WAS CHANGED.");
                    }
                    g_ps2_config_manager_selection =
                        (g_ps2_config_manager_selection + 10) % 11;
                }
                if (down && !old_down) {
                    if (g_ps2_patch_confirm_pending) {
                        g_ps2_patch_confirm_pending = 0;
                        snprintf(g_ps2_config_manager_message,
                                 sizeof(g_ps2_config_manager_message),
                                 "PATCH CONFIRMATION CANCELLED. NO FILE WAS CHANGED.");
                    }
                    g_ps2_config_manager_selection =
                        (g_ps2_config_manager_selection + 1) % 11;
                }
                if (cross && !old_cross) perform_ps2_config_manager_action();
            }
        } else if (g_screen == SCREEN_FPS_CONTROL) {
            const ps3scale_game_t *game = selected_game();
            const ps3scale_profile_t *profile = profile_for_game(game);
            int state;

            if ((cross && !old_cross) || (square && !old_square))
                g_details_cache_valid = 0;

            if (up && !old_up)
                g_fps_control_selection = 0;
            if (down && !old_down && profile)
                g_fps_control_selection = 1;

            if (cross && !old_cross) {
                if (g_profile_arm_running || g_profile_arm_done) {
                    set_details_message("WAIT FOR PROFILE ARMING TO FINISH.");
                } else if (g_profile_confirm_pending) {
                    set_details_message("PRESS TRIANGLE TO CONFIRM OR CIRCLE TO CANCEL.");
                } else if (!game || !game_is_ps3(game)) {
                    set_details_message("FPS CONTROL REQUIRES A SELECTED PS3 GAME.");
                } else if (g_fps_control_selection == 0) {
                    state = profile_script_state_for_game(game, profile);
                    if (state == 1 || state == -2) {
                        if (ps3scale_profile_script_remove(game->title_id) == 0)
                            set_details_message("ORIGINAL MODE SELECTED. PS3 SCALE PROFILE REMOVED.");
                        else
                            set_details_message("COULD NOT REMOVE THE PS3 SCALE PROFILE SCRIPT.");
                    } else if (state == -1) {
                        set_details_message("USER WEBMAN SCRIPT PROTECTED. NO FILE WAS CHANGED.");
                    } else {
                        set_details_message("ORIGINAL MODE SELECTED. NO PROFILE IS ARMED.");
                    }
                } else if (!profile) {
                    set_details_message("NO EXACT FPS PROFILE FOR THIS TITLE ID + APP VERSION.");
                } else if (strcmp(profile->trust, "B_COMMUNITY_MANUAL") == 0) {
                    set_details_message("MANUAL-ONLY PROFILE. AUTOMATIC APPLY IS DISABLED.");
                } else if (profile->patch_count != 1) {
                    set_details_message("MULTI-WRITE PROFILE. AUTOMATIC APPLY IS DISABLED.");
                } else {
                    state = profile_script_state_for_game(game, profile);
                    if (state == 1) {
                        set_details_message("PROFILE ALREADY ARMED FOR THE NEXT GAME START.");
                    } else if (state == -1) {
                        set_details_message("USER WEBMAN SCRIPT EXISTS. IT WILL NOT BE OVERWRITTEN.");
                    } else if (state == -2) {
                        set_details_message("STALE SCRIPT FOUND. PRESS SQUARE TO REMOVE IT FIRST.");
                    } else {
                        g_profile_confirm_pending = 1;
                        set_details_message("COMMUNITY FPS PATCH. TRIANGLE TO CONFIRM APPLY.");
                    }
                }
            }

            if (triangle && !old_triangle) {
                if (g_profile_confirm_pending) {
                    confirm_profile_install();
                } else {
                    set_details_message("SELECT A PROFILE AND PRESS CROSS BEFORE CONFIRMING.");
                }
            }

            if (square && !old_square) {
                if (g_profile_arm_running || g_profile_arm_done) {
                    set_details_message("WAIT FOR PROFILE ARMING TO FINISH.");
                } else if (!game || !game_is_ps3(game)) {
                    set_details_message("NO PS3 GAME SELECTED.");
                } else {
                    state = profile_script_state_for_game(game, profile);
                    if (state == 1 || state == -2) {
                        if (ps3scale_profile_script_remove(game->title_id) == 0)
                            set_details_message("PS3 SCALE ONE-LAUNCH PROFILE REMOVED.");
                        else
                            set_details_message("COULD NOT REMOVE THE PS3 SCALE PROFILE.");
                    } else if (state == -1) {
                        set_details_message("USER WEBMAN SCRIPT PROTECTED. NO FILE WAS CHANGED.");
                    } else {
                        set_details_message("NO PS3 SCALE PROFILE SCRIPT TO REMOVE.");
                    }
                }
                g_profile_confirm_pending = 0;
            }

            if ((circle && !old_circle) || (select && !old_select) ||
                (left && !old_left)) {
                if (g_profile_confirm_pending) {
                    g_profile_confirm_pending = 0;
                    set_details_message("FPS PROFILE APPLY CANCELLED.");
                } else {
                    g_screen = SCREEN_DETAILS;
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
            /* Left/right jump a result page while keeping the current row.
               The final short page is clamped so selection always remains
               visible; L1/R1 continue to move one result as before. */
            if (((left && !old_left) || (right && !old_right)) && matches) {
                size_t page_count =
                    (matches + PS3SCALE_ONLINE_PAGE_SIZE - 1u) /
                    PS3SCALE_ONLINE_PAGE_SIZE;
                size_t current_page =
                    (size_t)g_online_selected / PS3SCALE_ONLINE_PAGE_SIZE;
                size_t row =
                    (size_t)g_online_selected % PS3SCALE_ONLINE_PAGE_SIZE;
                size_t next_page = current_page;
                size_t next_selection;
                if (left && !old_left)
                    next_page = (current_page + page_count - 1u) % page_count;
                else if (right && !old_right)
                    next_page = (current_page + 1u) % page_count;
                next_selection = next_page * PS3SCALE_ONLINE_PAGE_SIZE + row;
                if (next_selection >= matches)
                    next_selection = matches - 1u;
                g_online_selected = (int)next_selection;
            }
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
                start_catalog_sync_worker(1);
            if (cross && !old_cross) {
                const ps3scale_profile_t *profile = NULL;
                const ps3scale_community_file_t *community = NULL;
                const ps3scale_ps2_config_entry_t *ps2_config = NULL;
                const ps3scale_game_t *match =
                    online_result_at((size_t)g_online_selected, &profile,
                                     &community, &ps2_config);
                if (community)
                    start_community_file_download(community, match);
                else
                    open_selected_online_match();
            }
            if (select && !old_select)
                (void)open_online_search_keyboard();
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
                    g_music_fade_ms = 1000;
                    g_animation_enabled = 1;
                    g_enhancement_persistent = 0;
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
                } else if (g_settings_selected == 8) {
                    g_music_fade_ms = cycle_music_fade_ms(g_music_fade_ms, 1);
                    (void)save_app_settings();
                } else if (g_settings_selected == 9) {
                    g_enhancement_persistent = !g_enhancement_persistent;
                    set_settings_message(g_enhancement_persistent ?
                        "NEW NCL FILES WILL BE KEPT PERMANENTLY." :
                        "NEW NCL FILES WILL BE TEMPORARY.");
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
                if (g_settings_selected == 8) {
                if (left && !old_left) {
                    g_music_fade_ms = cycle_music_fade_ms(g_music_fade_ms, -1);
                    (void)save_app_settings();
                }
                if (right && !old_right) {
                    g_music_fade_ms = cycle_music_fade_ms(g_music_fade_ms, 1);
                    (void)save_app_settings();
                }
            }
            if (g_settings_selected == 9 &&
                ((left && !old_left) || (right && !old_right))) {
                g_enhancement_persistent = !g_enhancement_persistent;
                set_settings_message(g_enhancement_persistent ?
                    "NEW NCL FILES WILL BE KEPT PERMANENTLY." :
                    "NEW NCL FILES WILL BE TEMPORARY.");
                (void)save_app_settings();
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
            if (up && !old_up)
                g_tools_selected = (g_tools_selected + 1) % 2;
            if (down && !old_down)
                g_tools_selected = (g_tools_selected + 1) % 2;
            if (cross && !old_cross)
                start_selected_companion_tool();
            if (triangle && !old_triangle)
                refresh_companion_tool_status();
            if (circle && !old_circle)
                g_screen = SCREEN_HOME;
        } else if (circle && !old_circle) {
            g_screen = SCREEN_HOME;
        }

        if (g_screen != screen_at_frame_start &&
            (g_screen == SCREEN_DETAILS || g_screen == SCREEN_FPS_CONTROL))
            g_details_cache_valid = 0;
        if ((g_screen == SCREEN_DETAILS || g_screen == SCREEN_FPS_CONTROL) &&
            !g_details_cache_valid)
            refresh_details_cache();
        if (g_screen == SCREEN_PS2_CONFIG && screen_at_frame_start != SCREEN_PS2_CONFIG)
            refresh_ps2_config_manager_cache();

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

        update_home_card_animations();
        update_scan_card_animations();
        update_online_row_animation();
        if (g_screen == SCREEN_SCAN || g_screen == SCREEN_DETAILS)
            update_selected_cover();
        if (g_screen == SCREEN_DETAILS)
            update_selected_preview();
        draw_ui();
    }

    /*
     * A launch request can still be in flight if the user exits while the
     * Companion Tools worker is waiting on the local webMAN socket. Join it
     * before tearing down the application and its shared state.
     */
    if (g_tool_launch_thread_started) {
        u64 thread_result = 0;
        (void)sysThreadJoin(g_tool_launch_thread, &thread_result);
        g_tool_launch_thread_started = 0;
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
    if (g_community_download_thread_started) {
        u64 thread_result = 0;
        (void)sysThreadJoin(g_community_download_thread, &thread_result);
        g_community_download_thread_started = 0;
    }
    ps3scale_ps2_config_db_free(&g_ps2_config_db);
    shutdown_audio_backend();
    sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
    clear_scan_cover_cache();
    if (g_cover_texture) ya2d_freeTexture(g_cover_texture);
    g_cover_texture = NULL;
    if (g_preview_before_texture) ya2d_freeTexture(g_preview_before_texture);
    g_preview_before_texture = NULL;
    if (g_preview_after_texture) ya2d_freeTexture(g_preview_after_texture);
    g_preview_after_texture = NULL;
    if (g_apollo_texture) ya2d_freeTexture(g_apollo_texture);
    g_apollo_texture = NULL;
    if (pad_ready) ya2d_deinit();
    return 0;
}