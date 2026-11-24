#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <sys/types.h>
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
#include "ps3scale/ps2_config_store.h"
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
    SCREEN_PS2_CONFIG,
    SCREEN_FPS_CONTROL,
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
static char g_ps2_config_download_type[PS3SCALE_PS2_CONFIG_TYPE_MAX];
static int g_ps2_config_download_replace = 0;
static char g_ps2_config_backup_path[PS3SCALE_PS2_CONFIG_STORE_PATH_MAX];
static char g_ps2_config_rollback_path[PS3SCALE_PS2_CONFIG_STORE_PATH_MAX];
static int g_ps2_config_manager_target_exists = 0;
static int g_ps2_config_manager_backup_exists = 0;
static int g_ps2_config_manager_backup_matches = 0;
static int g_ps2_config_manager_rollback_exists = 0;
static int g_ps2_config_manager_pending = 0;
static char g_ps2_config_manager_message[128] = "SQUARE: BACK UP THE CURRENT CONFIG FIRST.";
enum {
    PS2_CONFIG_CONFIRM_NONE = 0,
    PS2_CONFIG_CONFIRM_REPLACE = 1,
    PS2_CONFIG_CONFIRM_RESTORE = 2
};
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
static char g_search[64] = "";
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
#define PS3SCALE_SUPPORT_OUTBOX_PATH "/dev_hdd0/game/PS3SCALE1/USRDIR/support/outbox.txt"
/* Replace with the URL of the separate PS3 SCALE Supabase project after deployment. */
#define PS3SCALE_SUPPORT_URL "https://YOUR_PROJECT.supabase.co/functions/v1/ps3scale-support"
#define PS3SCALE_APP_VERSION "ALPHA 0.30"
#define PS3SCALE_PS2_CONFIG_INDEX_PATH "/dev_hdd0/game/PS3SCALE1/USRDIR/ps2_config_index.tsv"
#define PS3SCALE_PS2_CLASSICS_CONFIG_DIR "/dev_hdd0/game/PS2CONFIG/USRDIR/CONFIG/ENC"
#define PS3SCALE_PS2_CONFIG_DOWNLOAD_LIMIT (PS3SCALE_PS2_CONFIG_MAX_BYTES * 2u)
#define PS3SCALE_PS2_CONFIG_BACKUP_DIR "/dev_hdd0/game/PS3SCALE1/USRDIR/ps2_config_backups"
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
static void start_ps2_config_download(int allow_replace)
{
    const ps3scale_game_t *game = selected_game();
    const ps3scale_ps2_config_entry_t *entry = ps2_config_for_game(game);
    struct stat st;
    char url[512];
    char target[PS3SCALE_PATH_MAX];
    char temporary[PS3SCALE_PATH_MAX];
    int written;
    size_t max_bytes;

    if (g_network_task_running ||
        g_ps2_config_download_running || g_ps2_config_download_done ||
        g_ps2_config_download_thread_started) {
        set_details_message("WAIT FOR THE CURRENT PS2 CONFIG TASK TO FINISH.");
        snprintf(g_ps2_config_manager_message,
                 sizeof(g_ps2_config_manager_message),
                 "WAIT FOR THE CURRENT PS2 CONFIG TASK TO FINISH.");
        return;
    }
    if (!game || !entry) {
        set_details_message("NO VERIFIED PS2 CONFIG MATCHES THIS GAME ID.");
        snprintf(g_ps2_config_manager_message,
                 sizeof(g_ps2_config_manager_message),
                 "NO VERIFIED PS2 CONFIG MATCHES THIS GAME ID.");
        return;
    }
    if (ps3scale_ps2_config_db_build_url(entry, url, sizeof(url)) != 0) {
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
    } else if (game->kind != PS3SCALE_GAME_PS2_ISO) {
        set_details_message("THIS CONFIG TYPE REQUIRES A STANDARD PS2 ISO.");
        return;
    }

    if (ps2_config_store_paths(
            game, entry, target, sizeof(target),
            g_ps2_config_backup_path, sizeof(g_ps2_config_backup_path),
            g_ps2_config_rollback_path,
            sizeof(g_ps2_config_rollback_path)) != 0) {
        set_details_message("PS2 CONFIG TARGET PATH IS INVALID OR TOO LONG.");
        return;
    }

    max_bytes = ps2_config_type_limit(entry->type);
    if (stat(target, &st) == 0) {
        if (!allow_replace) {
            set_details_message("CONFIG ALREADY EXISTS. OPEN CONFIG MANAGER TO BACK UP FIRST.");
            snprintf(g_ps2_config_manager_message,
                     sizeof(g_ps2_config_manager_message),
                     "CONFIG EXISTS. BACK UP IT BEFORE REPLACING.");
            return;
        }
        if (ps3scale_ps2_config_store_file_matches(
                target, g_ps2_config_backup_path, max_bytes) != 1) {
            set_details_message("BACKUP MISSING OR CONFIG CHANGED. REPLACEMENT BLOCKED.");
            snprintf(g_ps2_config_manager_message,
                     sizeof(g_ps2_config_manager_message),
                     "BACKUP DOES NOT MATCH CURRENT CONFIG. NO REPLACEMENT.");
            return;
        }
    } else if (allow_replace) {
        /* If the target disappeared after confirmation, a normal first install
         * is safe and does not need replacement permission. */
        allow_replace = 0;
    }

    written = snprintf(temporary, sizeof(temporary),
                       "%s.ps3scale.tmp", target);
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
    g_ps2_config_download_replace = allow_replace;
    g_ps2_config_download_result = -1;
    g_ps2_config_downloaded = 0;
    g_ps2_config_download_total = 0;
    g_ps2_config_download_done = 0;
    g_ps2_config_download_running = 1;
    g_network_task_running = 1;

    if (sysThreadCreate(&g_ps2_config_download_thread,
                        ps2_config_download_worker, NULL,
                        1001, 64 * 1024, THREAD_JOINABLE,
                        "ps3scale_ps2cfg") != 0) {
        g_ps2_config_download_running = 0;
        g_ps2_config_downloaded = 0;
        g_ps2_config_download_total = 0;
        g_network_task_running = 0;
        g_ps2_config_download_replace = 0;
        set_details_message("COULD NOT START PS2 CONFIG DOWNLOAD WORKER.");
        return;
    }
    g_ps2_config_download_thread_started = 1;
    set_details_message(allow_replace ?
        "REPLACING CONFIG. ORIGINAL BACKUP WILL BE PRESERVED." :
        "DOWNLOADING VERIFIED PS2 CONFIG. ORIGINAL GAME IMAGE IS UNCHANGED.");
    snprintf(g_ps2_config_manager_message,
             sizeof(g_ps2_config_manager_message),
             allow_replace ? "REPLACING CONFIG FROM VERIFIED MATCH." :
                             "DOWNLOADING VERIFIED CONFIG FILE.");
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
    struct stat st;
    (void)arg;

    if (ps3scale_online_download_limited_progress(
            g_community_download_url,
            g_community_download_temp_path,
            PS3SCALE_COMMUNITY_INDEX_DOWNLOAD_LIMIT,
            &g_community_downloaded, &g_community_download_total) == 0) {
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

    if (result != 0)
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
    } else {
        snprintf(g_online_message, sizeof(g_online_message),
                 "COMMUNITY NCL DOWNLOAD FAILED. CHECK NETWORK AND SOURCE.");
    }
    g_community_download_expected_size = 0;
    g_community_download_done = 0;
    g_community_downloaded = 0;
    g_community_download_total = 0;
}

static void service_ps2_config_download(void)
{
    int replaced;
    if (!g_ps2_config_download_done)
        return;

    if (g_ps2_config_download_thread_started) {
        u64 thread_result = 0;
        (void)sysThreadJoin(g_ps2_config_download_thread, &thread_result);
        g_ps2_config_download_thread_started = 0;
    }

    replaced = g_ps2_config_download_replace;
    if (g_ps2_config_download_result == 0) {
        set_details_message(replaced ?
            "PS2 CONFIG REPLACED. ORIGINAL BACKUP WAS PRESERVED." :
            "PS2 CONFIG SAVED SEPARATELY. ORIGINAL GAME IMAGE IS UNCHANGED.");
        snprintf(g_ps2_config_manager_message,
                 sizeof(g_ps2_config_manager_message),
                 replaced ? "CONFIG REPLACED. ORIGINAL BACKUP IS PRESERVED." :
                            "CONFIG INSTALLED. ORIGINAL GAME IMAGE IS UNCHANGED.");
    } else if (g_ps2_config_download_result == -2) {
        set_details_message("CONFIG ALREADY EXISTS. IT WAS NOT OVERWRITTEN.");
        snprintf(g_ps2_config_manager_message,
                 sizeof(g_ps2_config_manager_message),
                 "CONFIG EXISTS. BACK UP IT BEFORE REPLACING.");
    } else if (g_ps2_config_download_result == -4) {
        set_details_message("DOWNLOADED CONFIG FAILED VALIDATION. FILE REMOVED.");
        snprintf(g_ps2_config_manager_message,
                 sizeof(g_ps2_config_manager_message),
                 "DOWNLOAD FAILED VALIDATION. NO CONFIG WAS INSTALLED.");
    } else if (g_ps2_config_download_result == -6) {
        set_details_message("BACKUP MISSING OR CONFIG CHANGED. REPLACEMENT BLOCKED.");
        snprintf(g_ps2_config_manager_message,
                 sizeof(g_ps2_config_manager_message),
                 "BACKUP MISSING OR DOES NOT MATCH CURRENT CONFIG.");
    } else if (g_ps2_config_download_result == -7) {
        set_details_message("BACKUP CONFLICT. NO CONFIG WAS OVERWRITTEN.");
        snprintf(g_ps2_config_manager_message,
                 sizeof(g_ps2_config_manager_message),
                 "ROLLBACK CONFLICT. NO CONFIG WAS OVERWRITTEN.");
    } else if (g_ps2_config_download_result == -5) {
        set_details_message("COULD NOT SAVE CONFIG. CHECK THE GAME FOLDER.");
        snprintf(g_ps2_config_manager_message,
                 sizeof(g_ps2_config_manager_message),
                 "COULD NOT SAVE CONFIG. CURRENT FILE WAS LEFT IN PLACE.");
    } else {
        set_details_message("PS2 CONFIG DOWNLOAD FAILED. GAME FILES WERE NOT CHANGED.");
        snprintf(g_ps2_config_manager_message,
                 sizeof(g_ps2_config_manager_message),
                 "PS2 CONFIG DOWNLOAD FAILED. NO FILE WAS REPLACED.");
    }

    g_ps2_config_download_done = 0;
    g_ps2_config_downloaded = 0;
    g_ps2_config_download_total = 0;
    g_ps2_config_download_replace = 0;
    g_details_cache_valid = 0;
    refresh_ps2_config_manager_state();
}

static int ps2_config_target_exists(const ps3scale_game_t *game,
                                    const ps3scale_ps2_config_entry_t *entry)
{
    char target[PS3SCALE_PATH_MAX];
    return ps2_config_target_path(game, entry, target, sizeof(target)) == 0 &&
           file_exists_readonly(target);
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

static void draw_keyboard(void)
{
    int i;
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
    draw_text(54.0f, 120.0f, 0x29243fff, g_search[0] ? g_search : " ");

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
        int exact_local_game =
            selected && game_is_ps3(selected) &&
            selected->title_id[0] && selected->app_ver[0] &&
            strncmp(selected->path, "/dev_hdd0/", 10) == 0 &&
            ps3scale_community_file_matches_exact_game(
                selected_community, selected);
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
        if (selected &&
            ps3scale_community_file_matches_exact_game(selected_community,
                                                        selected)) {
            draw_text(414.0f, 357.0f, 0x26794bff,
                      "MATCH: EXACT TITLE ID / APP VERSION");
        } else if (selected) {
            draw_text(414.0f, 357.0f, 0xb27b35ff,
                      "MATCH: REVIEW TITLE ID / VERSION");
        } else {
            draw_text(414.0f, 357.0f, 0xb27b35ff,
                      "SEARCH RESULT: NO LOCAL GAME MATCH");
        }
        snprintf(line, sizeof(line), "SIZE: %u KB | %s",
                 (unsigned)((selected_community->file_size + 1023u) / 1024u),
                 can_download ? "NCL GRAPHICS PATCH" : "NOT INSTALLABLE");
        draw_text(414.0f, 381.0f, 0x514b69ff, line);
        if (can_download) {
            draw_text(414.0f, 393.0f, 0x875be8ff,
                      "CROSS: INSTALL TO ARTEMIS USERLIST");
        } else if (!selected) {
            draw_text(414.0f, 393.0f, 0xb27b35ff,
                      "CROSS: SCAN THE EXACT GAME FIRST");
        } else if (!g_artemis_installed) {
            draw_text(414.0f, 393.0f, 0xb27b35ff,
                      "INSTALL ARTEMIS PS3 FIRST");
        } else if (strcmp(selected_community->category, "GRAPHICS_FPS") != 0) {
            draw_text(414.0f, 393.0f, 0xb27b35ff,
                      "NOT A GRAPHICS / FPS PATCH");
        } else if (!exact_local_game) {
            draw_text(414.0f, 393.0f, 0xb27b35ff,
                      "EXACT TITLE ID + VERSION REQUIRED");
        } else {
            draw_text(414.0f, 393.0f, 0xb27b35ff,
                      "ONLY NCL PATCH FILES ARE SUPPORTED");
        }
        draw_text(414.0f, 414.0f, 0xb27b35ff,
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
    draw_rounded_rect(203.0f, 246.0f, 290.0f, 21.0f, 7.0f,
                      (profile || ps2_config) ? 0xe5f5eaff : 0xfff3e5ff);
    set_ui_font_size(7, 11);
    draw_text(212.0f, 251.0f,
              (profile || ps2_config) ? 0x26794bff : 0xb27b35ff,
              profile ? ((strcmp(profile->trust, "B_COMMUNITY") == 0 &&
                           profile->patch_count == 1) ?
                          "MATCHED PROFILE - AUTO APPLY ON CROSS" :
                          "MANUAL PROFILE - REVIEW FIRST") :
              ps2_config ? "MATCHED PS2 CONFIG - REVIEW FIRST" :
                           "NO MATCHED FILE - PLAY UNMODIFIED");
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
    snprintf(line, sizeof(line), "Version: %.16s",
             game_is_ps3(game) ?
             (game->app_ver[0] ? game->app_ver : "Unknown - rescan") :
             "PS2 configuration");
    draw_text(204.0f, 202.0f, 0x514b69ff, line);
    snprintf(line, sizeof(line), "Location: %.25s", game->path);
    draw_text(204.0f, 228.0f, 0x77718fff, line);
    draw_text(204.0f, 277.0f, 0x29243fff, "Enhancement Status");
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
                  "PS2 CONFIGURATION AVAILABLE");
        draw_text(544.0f, 304.0f, 0x514b69ff,
                  "Compatibility settings are game-specific.");
        draw_text(544.0f, 323.0f, 0x514b69ff,
                  "This is NOT an HD texture replacement.");
        draw_text(544.0f, 342.0f, 0x875be8ff,
                  "No matched before/after images are available.");
    } else {
        draw_text(544.0f, 290.0f, 0xb27b35ff,
                  "NO VERIFIED ENHANCEMENT AVAILABLE");
        draw_text(544.0f, 311.0f, 0x514b69ff,
                  "The game will start without changes.");
        draw_text(544.0f, 334.0f, 0x875be8ff,
                  "No matched comparison images are available.");
    }

    draw_soft_button(20.0f, 400.0f, 250.0f, 44.0f,
                     game_is_ps3(game) ?
                     (profile ? "TRIANGLE: ARM PROFILE" : "NO EXACT FPS PROFILE") :
                     (ps2_config ? "TRIANGLE: DOWNLOAD CONFIG" : "NO CONFIG MATCH"),
                     1);
    draw_soft_button(282.0f, 400.0f, 250.0f, 44.0f,
                     game_is_ps3(game) ? "SQUARE: FPS CONTROL" :
                                         "SQUARE: CONFIG MANAGER", 0);
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
    const ps3scale_game_t *game = selected_game();
    const ps3scale_ps2_config_entry_t *entry = ps2_config_for_game(game);
    char target[PS3SCALE_PATH_MAX];
    char line[192];
    const char *id;

    draw_page_header("PS2 CONFIG Manager",
                     "Back up, install and restore per-title compatibility files");
    draw_soft_card(20.0f, 88.0f, 814.0f, 294.0f, 0);

    if (!game || (game->kind != PS3SCALE_GAME_PS2_ISO &&
                  game->kind != PS3SCALE_GAME_PS2_CLASSIC)) {
        draw_text(44.0f, 132.0f, 0xb27b35ff,
                  "Select a PS2 ISO or PS2 Classic first.");
        draw_text(24.0f, PS3SCALE_UI_FOOTER_SINGLE_Y, 0x875be8ff,
                  "CIRCLE: BACK");
        set_ui_font_size(14, 22);
        return;
    }

    id = game->disc_id[0] ? game->disc_id : game->title_id;
    snprintf(line, sizeof(line), "GAME: %.54s", game->title);
    set_ui_font_size(10, 15);
    draw_text(44.0f, 112.0f, 0x29243fff, line);
    set_ui_font_size(7, 11);
    snprintf(line, sizeof(line), "GAME ID: %.24s", id[0] ? id : "NOT READ");
    draw_text(44.0f, 146.0f, 0x514b69ff, line);
    snprintf(line, sizeof(line), "SOURCE: %.18s",
             entry ? entry->type : "NO MATCH");
    draw_text(44.0f, 170.0f, 0x514b69ff, line);

    if (ps2_config_target_path(game, entry, target, sizeof(target)) == 0)
        snprintf(line, sizeof(line), "TARGET: %.78s", target);
    else
        snprintf(line, sizeof(line), "TARGET: INVALID OR UNSUPPORTED PATH");
    draw_text(44.0f, 194.0f, 0x77718fff, line);

    snprintf(line, sizeof(line), "CURRENT CONFIG: %s",
             g_ps2_config_manager_target_exists ? "PRESENT" : "NOT INSTALLED");
    draw_text(44.0f, 228.0f, 0x29243fff, line);
    snprintf(line, sizeof(line), "ORIGINAL BACKUP: %s",
             !g_ps2_config_manager_backup_exists ? "MISSING" :
             (g_ps2_config_manager_backup_matches ? "MATCHES CURRENT FILE" :
                                                    "PRESERVED; CURRENT FILE CHANGED"));
    draw_text(44.0f, 252.0f,
              g_ps2_config_manager_backup_exists ? 0x26794bff : 0xb27b35ff,
              line);
    snprintf(line, sizeof(line), "PRE-RESTORE COPY: %s",
             g_ps2_config_manager_rollback_exists ? "AVAILABLE" : "NOT CREATED");
    draw_text(44.0f, 276.0f, 0x514b69ff, line);

    draw_text(44.0f, 310.0f, 0x77718fff,
              "Backups are stored separately; ISO and EBOOT files are never edited.");
    draw_text(44.0f, 332.0f, 0x77718fff,
              "Replacing a config requires an exact current-file backup.");
    draw_soft_button(20.0f, 400.0f, 194.0f, 44.0f,
                     "SQUARE: BACKUP", 0);
    draw_soft_button(220.0f, 400.0f, 194.0f, 44.0f,
                     g_ps2_config_manager_pending == PS2_CONFIG_CONFIRM_REPLACE ?
                     "TRIANGLE: CONFIRM" : "TRIANGLE: INSTALL", 1);
    draw_soft_button(420.0f, 400.0f, 194.0f, 44.0f,
                     g_ps2_config_manager_pending == PS2_CONFIG_CONFIRM_RESTORE ?
                     "CROSS: CONFIRM" : "CROSS: RESTORE", 0);
    draw_soft_button(620.0f, 400.0f, 214.0f, 44.0f,
                     "CIRCLE: BACK", 0);
    set_ui_font_size(7, 11);
    draw_text(24.0f, PS3SCALE_UI_FOOTER_PRIMARY_Y, 0x875be8ff,
              g_ps2_config_manager_message);
    draw_text(24.0f, PS3SCALE_UI_FOOTER_SECONDARY_Y, 0x77718fff,
              "SQUARE BACKS UP | TRIANGLE INSTALLS / REPLACES | CROSS RESTORES | CIRCLE BACK");
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
    static const char *actions[2] = { "Report an Issue", "Send Feedback" };
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
    draw_soft_button(252.0f, 362.0f, 220.0f, 42.0f,
                     actions[1], g_help_selected == 1);
    draw_soft_button(486.0f, 362.0f, 220.0f, 42.0f,
                     actions[0], g_help_selected == 0);
    set_ui_font_size(6, 9);
    draw_text(38.0f, 414.0f, 0x77718fff,
              "Community files are saved as data only; they are never auto-executed.");
    set_ui_font_size(7, 11);
    draw_text(24.0f, PS3SCALE_UI_FOOTER_SINGLE_Y, 0x77718fff,
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
        draw_text(60.0f, y + 9.0f,
                  selected ? 0x875be8ff : 0x514b69ff, labels[i]);
        set_ui_font_size(7, 11);
        if (i == 0)
            draw_text(60.0f, y + 38.0f,
                      g_support_title[0] ? 0x29243fff : 0x77718fff,
                      g_support_title[0] ? g_support_title :
                                           "Press CROSS to enter a title");
        else if (i == 1)
            draw_text(60.0f, y + 38.0f,
                      g_support_details[0] ? 0x29243fff : 0x77718fff,
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
    draw_text(24.0f, PS3SCALE_UI_FOOTER_SINGLE_Y, 0x77718fff,
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
        "\"app_version\":\"%s\"}",
        g_support_is_feedback ? "feedback" : "issue",
        title, details, model, firmware, platform, PS3SCALE_APP_VERSION);
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
    g_network_task_running = 0;
}

static void start_support_send(void)
{
    int rc;

    if (strstr(PS3SCALE_SUPPORT_URL, "YOUR_PROJECT") != NULL) {
        snprintf(g_support_status, sizeof(g_support_status),
                 "LOCAL SAVE OK; ONLINE OFF.");
        return;
    }
    if (g_network_task_running ||
        g_catalog_sync_running || g_catalog_thread_started ||
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
    fprintf(fp, "APP VERSION: %s\n", PS3SCALE_APP_VERSION);
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
        case SCREEN_PS2_CONFIG: draw_ps2_config_manager(); break;
        case SCREEN_FPS_CONTROL: draw_fps_control(); break;
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
                if (square && !old_square && game && game_is_ps3(game)) {
                    g_profile_confirm_pending = 0;
                    g_fps_control_selection = 0;
                    g_screen = SCREEN_FPS_CONTROL;
                } else if (square && !old_square && game &&
                           (game->kind == PS3SCALE_GAME_PS2_ISO ||
                            game->kind == PS3SCALE_GAME_PS2_CLASSIC)) {
                    g_ps2_config_manager_pending = PS2_CONFIG_CONFIRM_NONE;
                    snprintf(g_ps2_config_manager_message,
                             sizeof(g_ps2_config_manager_message),
                             "BACK UP THE CURRENT CONFIG BEFORE REPLACING IT.");
                    refresh_ps2_config_manager_state();
                    g_screen = SCREEN_PS2_CONFIG;
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
                            start_ps2_config_download(0);
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
            const ps3scale_game_t *game = selected_game();
            const ps3scale_ps2_config_entry_t *entry = ps2_config_for_game(game);

            if ((circle && !old_circle) || (select && !old_select) ||
                (left && !old_left)) {
                g_ps2_config_manager_pending = PS2_CONFIG_CONFIRM_NONE;
                g_screen = SCREEN_DETAILS;
            } else if (square && !old_square) {
                backup_selected_ps2_config();
                g_ps2_config_manager_pending = PS2_CONFIG_CONFIRM_NONE;
            } else if (triangle && !old_triangle) {
                refresh_ps2_config_manager_state();
                if (!entry) {
                    snprintf(g_ps2_config_manager_message,
                             sizeof(g_ps2_config_manager_message),
                             "NO VERIFIED COMMUNITY CONFIG MATCHES THIS GAME.");
                    g_ps2_config_manager_pending = PS2_CONFIG_CONFIRM_NONE;
                } else if (!g_ps2_config_manager_target_exists) {
                    g_ps2_config_manager_pending = PS2_CONFIG_CONFIRM_NONE;
                    start_ps2_config_download(0);
                } else if (g_ps2_config_manager_backup_matches) {
                    if (g_ps2_config_manager_pending ==
                        PS2_CONFIG_CONFIRM_REPLACE) {
                        g_ps2_config_manager_pending = PS2_CONFIG_CONFIRM_NONE;
                        start_ps2_config_download(1);
                    } else {
                        g_ps2_config_manager_pending =
                            PS2_CONFIG_CONFIRM_REPLACE;
                        snprintf(g_ps2_config_manager_message,
                                 sizeof(g_ps2_config_manager_message),
                                 "TRIANGLE AGAIN TO REPLACE. ORIGINAL BACKUP IS READY.");
                    }
                } else if (g_ps2_config_manager_backup_exists) {
                    g_ps2_config_manager_pending = PS2_CONFIG_CONFIRM_NONE;
                    snprintf(g_ps2_config_manager_message,
                             sizeof(g_ps2_config_manager_message),
                             "BACKUP DOES NOT MATCH CURRENT FILE. RESTORE ORIGINAL FIRST.");
                } else {
                    g_ps2_config_manager_pending = PS2_CONFIG_CONFIRM_NONE;
                    snprintf(g_ps2_config_manager_message,
                             sizeof(g_ps2_config_manager_message),
                             "PRESS SQUARE TO BACK UP THE CURRENT CONFIG FIRST.");
                }
            } else if (cross && !old_cross) {
                refresh_ps2_config_manager_state();
                if (!g_ps2_config_manager_backup_exists) {
                    g_ps2_config_manager_pending = PS2_CONFIG_CONFIRM_NONE;
                    snprintf(g_ps2_config_manager_message,
                             sizeof(g_ps2_config_manager_message),
                             "NO ORIGINAL BACKUP IS AVAILABLE TO RESTORE.");
                } else if (g_ps2_config_manager_pending ==
                           PS2_CONFIG_CONFIRM_RESTORE) {
                    g_ps2_config_manager_pending = PS2_CONFIG_CONFIRM_NONE;
                    restore_selected_ps2_config();
                } else {
                    g_ps2_config_manager_pending =
                        PS2_CONFIG_CONFIRM_RESTORE;
                    snprintf(g_ps2_config_manager_message,
                             sizeof(g_ps2_config_manager_message),
                             "CROSS AGAIN TO RESTORE THE ORIGINAL BACKUP.");
                }
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