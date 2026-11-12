#!/usr/bin/env python3
"""Static UI contract checks for PS3 SCALE's PS3-only UI."""
import pathlib
import sys

source_path = pathlib.Path(sys.argv[1])
src = source_path.read_text(encoding="utf-8")

def section(start, end):
    a = src.index(start)
    b = src.index(end, a + len(start))
    return src[a:b]

# Search uses the in-app controller keyboard; no native PS3 OSK lifecycle remains.
main = src[src.index("int main(int argc, char *argv[])"):]
assert "SCREEN_KEYBOARD" in src
assert "keyboard_move(0)" in main
assert "keyboard_press()" in main
assert "keyboard_cancel()" in main
assert "(select && !old_select) || (start && !old_start)" in main
assert "ps3scale_locale_keyboard_key" in src
assert "oskLoadAsync" not in src
assert "oskGetInputText" not in src
assert "g_osk_active" not in src

# First-run language choice and persisted language setting.
assert "SCREEN_LANGUAGE" in src
assert "CHOOSE LANGUAGE" in src
assert "اختر اللغة" in src
assert "Elige el idioma" in src
assert 'fprintf(fp, "language=%d\\n", g_language);' in src
assert "g_settings_selected == 6" in main

# Startup sequence: local checks and a background library scan with progress.
assert "SCREEN_BOOT" in src
assert "draw_boot" in src
assert "boot_scan_worker" in src
assert "start_boot_scan" in src
assert "SCANNING LOCAL GAME LIBRARY" in src
assert "APP FILE CHECK: OK" in src

# Arabic/Spanish text uses a dedicated extended font while ASCII keeps
# the original readable bitmap font. Mixed strings switch font per glyph.
assert "ps3scale_locale_translate" in src
assert "ps3scale_locale_to_glyphs" in src
assert "ps3scale_locale_glyph_font_index" in src
assert "font_bitmap_z.h" in src
assert "arabic_glyph_map.h" not in src  # mapping is isolated in locale.c
font_loader = section("static int load_extended_font(void)", "static void draw_text")
assert "ps3scale_font8x12" in font_loader
assert "PS3SCALE_FONT_EXTENDED_FIRST" in font_loader
assert "g_extended_font_bitmap + extended_offset" in font_loader
assert "build_extended_font_metrics();" in font_loader
assert "BIT7_FIRST_PIXEL" in font_loader
metrics = section("static void build_extended_font_metrics(void)", "static void set_ui_font_size")
assert "0x80u >> (col & 7u)" in metrics
assert "g_extended_glyph_min_x" in metrics
assert "g_extended_glyph_ink_width" in metrics
draw_text_impl = section("static void draw_text", "static int open_background_music")
assert "SetCurrentFont(font_index)" in draw_text_impl
assert "DrawChar(cursor - ((float)min_x * scale_x)" in draw_text_impl
assert "g_ui_font_height * 27 + 10" in draw_text_impl
assert "arabic_width = (g_ui_font_width * 18 + 7) / 14;" in draw_text_impl
assert "SetFontSize(arabic_width, arabic_height)" in draw_text_impl
assert "ps3scale_locale_glyphs_joined(glyph, next_glyph)" in draw_text_impl
assert "ps3scale_locale_is_arabic_glyph" in draw_text_impl
assert "cursor -= scale_x * 4.0f" in draw_text_impl
assert "cursor = DrawString(cursor, y, one) - 2.5f" not in draw_text_impl
assert src.count("SetFontSize(") == 3  # wrapper plus Arabic-size set/restore
assert draw_text_impl.count("SetFontSize(") == 2

# Library uses a full-width 4x2 cover grid, real cached covers and working filters.
scan = section("static void draw_scan(void)", "static void draw_details(void)")
assert "draw_soft_card(20.0f, 88.0f, 150.0f, 356.0f" in scan
assert "draw_soft_card(x, y, 150.0f, 148.0f, selected)" in scan
assert "ya2d_drawTextureEx(g_scan_cover_textures[i]" in scan
assert "draw_library_filter_item" in scan
assert "Favorites" in scan and "Recently Added" in scan
assert "SQUARE: FAVORITE" in scan
assert "PS3SCALE_SCAN_GRID_COUNT" in scan
assert "No games match this filter." in scan

# Settings retain the expected maintenance/audio actions and add language selection.
settings = section("static void draw_settings(void)", "static void draw_tools(void)")
for token in ("Music", "Music Volume", "Animations (UI & Background)",
              "Check for USB Update", "Rescan Game Library",
              "Restore Default Settings", "Language", "Music Playlist"):
    assert token in settings, f"Missing settings entry: {token}"
assert "COPY MUSIC TO INTERNAL HDD" not in settings
assert "copy_music_to_internal" not in src
assert "AUDIO ENGINE:" not in settings
assert "g_music_path" not in src
assert "mpg123_open(g_mpg123, path)" in src
assert "PS3SCALE_MUSIC1_PATH" in src
assert "PS3SCALE_MUSIC2_PATH" in src
assert "music_mode=%d" in src
assert "music_next=%d" in src
assert "open_music_track(next_track)" in src
assert "mpg123_tell(g_mpg123)" in src
assert "mpg123_length(g_mpg123)" in src
assert "music_sample_fade" in src
assert "Keep this block on one track" in src
assert "AUTO: %s > %s" in settings
assert "draw_ps_symbol(407.0f, 22.0f" in src
assert "MUSIC 1: BIOS - ZORROVIAN (USB IMPORT)" in settings
assert "MUSIC 2: GTA V (BUILT IN)" in settings
assert "START GAME" in src
assert "start_selected_game()" in main
assert "ps3scale_webman_start_game(&g_game_launch_target)" in src
assert "game_launch_worker" in src
assert "service_game_launch();" in main
assert "sysThreadCreate(&g_game_launch_thread" in src
assert "profile_arm_worker" in src
assert "service_profile_arm();" in main
assert "sysThreadCreate(&g_profile_arm_thread" in src
assert "find_installed_game(game)" in src
assert "NO PROFILE FOR THIS TITLE. GAME WILL START UNMODIFIED." in src
assert "START UNMODIFIED" in src
assert "APP VERSION:" in src
assert "if (g_music_enabled) {" in main
assert "g_music_start_track = g_music_next_track;" in main
assert "g_music_next_track = (g_music_start_track == 1) ? 2 : 1;" in main
assert "set_background_music_enabled(0);" in main
assert "set_background_music_enabled(1);" in main
assert "g_language = LANGUAGE_ENGLISH;" in main
assert "SETTINGS RESTORED TO DEFAULTS." in main
assert "shutdown_audio_backend();" in main
assert "BACKGROUND MUSIC STARTED." in main
assert "PS3SCALE_SETTINGS_PATH" in src
assert "settings.cfg" in src

# Five primary home tiles; Companion Tools remains reachable through SELECT.
home = section("static void draw_home(void)", "static void draw_online_database(void)")
for token in ("Scan Games", "Online DB", "Settings", "Help", "About",
              "SELECT: COMPANION TOOLS"):
    assert token in home
assert "static const char *items[5]" in home
assert "for (i = 0; i < 5; i++)" in home
assert "g_home_selected = (g_home_selected + 1) % 5;" in main
assert "SCREEN_HELP" in src and "SCREEN_SUPPORT_FORM" in src
help_ui = section("static void draw_help(void)", "static void draw_about(void)")
assert "Report an Issue" in help_ui and "Send Feedback" in help_ui
assert "A quick step-by-step guide to PS3 SCALE" in help_ui
assert "PS3SCALE_SUPPORT_OUTBOX_PATH" in src
assert "SAVED TO LOCAL OUTBOX" in src
assert "open_support_keyboard(0)" in main
assert "open_support_keyboard(1)" in main
assert "ya2d_loadPNGfromFile(PS3SCALE_APOLLO_LOGO_PATH)" in main
assert "ya2d_freeTexture(g_apollo_texture)" in main

# Read-only hardware detection reports firmware and the Platform ID without console IDs.
assert "ps3scale_hardware_detect(&g_hardware_info)" in main
about = section("static void draw_about(void)", "static void draw_ui(void)")
for token in ("Hardware Information", "Detected model:", "Console type:",
              "Firmware:", "Platform ID:", "Hardware information is unavailable."):
    assert token in about
hardware_detect_path = source_path.with_name("hardware_detect.c")
hardware_detect_src = hardware_detect_path.read_text(encoding="utf-8")
assert "lv2syscall1(387" in hardware_detect_src
assert "lv2syscall1(985" in hardware_detect_src
assert "ps3scale_hardware_info_parse" in hardware_detect_src
assert "ps3scale_console_type_name" in about
assert "IDPS" not in hardware_detect_src and "PSID" not in hardware_detect_src

# Home animation is true 3D but remains low-complexity and slow.
ambient = section("static void draw_home_ambient(void)", "static void set_settings_message")
assert "MatrixProjPerspective" in ambient
assert "MatrixRotationY" in ambient
assert "TINY3D_LINE_STRIP" in ambient
assert "draw_ambient_ring" in ambient
assert "draw_ambient_satellite" in ambient
assert "g_frame_counter % 21600ul" in ambient
assert "sinf(phase)" in ambient
assert "for (i = 0; i < 32; i++)" in ambient
assert "MatrixScale(2.0f, 2.0f, 2.0f)" in ambient
assert "const float center_x = -5.8f;" in ambient
assert "case 0xffffff00u: rgb = 0x29243f00u;" in src
assert 'sscanf(line, "animation_v2=%d"' in src
assert 'fprintf(fp, "animation_v2=%d\\n"' in src
draw_ui_impl = section("static void draw_ui(void)", "static int compare_game_titles")
assert "if (g_animation_enabled)" in draw_ui_impl
assert "draw_home_ambient();" in draw_ui_impl
assert "tiny3d_AlphaTest(1, 0x10, TINY3D_ALPHA_FUNC_GEQUAL);" in draw_ui_impl
assert "tiny3d_BlendFunc(1," in draw_ui_impl
assert "TINY3D_CLEAR_ZBUFFER" in draw_ui_impl

# Library sorting and companion-tool boundaries remain intact.
assert "qsort(g_games.items" in src
assert "SCREEN_TOOLS" in src
assert "APOLLO SAVE TOOL" in src
assert "ARTEMIS PS3" in src
assert "webMAN MOD" in src
assert "refresh_companion_tool_status" in src
assert "shutdown_audio_backend();" in main
assert "PS3SCALE_MUSIC_PATH" in src

# Online graphics database: HTTPS sync, exact version matching, and a dedicated UI.
assert "ONLINE GAME DATABASE" in src
assert "SCREEN_ONLINE_DB" in src
assert "PS3SCALE_CATALOG_URL" in src
assert "PS3SCALE_COMMUNITY_INDEX_URL" in src
assert "ps3scale_community_index_load" in src
assert "ps3scale_community_file_matches_game" in src
assert "ONLINE GAME DATABASE" in src
assert "APPLY: REFERENCE ONLY - NOT INSTALLED" in src
assert "SQUARE FILTER" in src
assert "TRIANGLE SEARCH" in src
assert "L1/R1 PREV/NEXT" in src
assert "GRAPHICS + EXACT" in src
assert "online_filter_exact_only" in src
assert "ps3scale_community_file_matches_exact_game" in src
assert "ps3scale_community_region_label" in src
assert "APPLY: NOT A GRAPHICS PROFILE" in src
assert "start_catalog_sync_worker(0)" in src
assert "start_catalog_sync_worker(1)" in main
assert "ps3scale_catalog_match_game" in src
assert "EXACT ONLINE PROFILE FOUND" in src
assert "ONE-LAUNCH PROFILE ARMED. START THIS GAME." in src
assert "CHECKING WEBMAN AND ARMING PROFILE." in src
assert "APPLY: REVIEW + CONFIRM IN DETAILS" in src
assert "STALE PROFILE SCRIPT. REMOVE IT BEFORE STARTING." in src
assert "MULTI-WRITE PROFILE DISABLED FOR AUTO APPLY SAFETY" in src
assert "cache_selected_online_profile" not in src
assert "cache_all_online_profiles" not in src
assert "profile_script_state_for_game" in src
assert "profile ? profile->patches : NULL" in src
assert "MOUNT THE EXACT GAME VERSION BEFORE ARMING THIS PROFILE." in src
assert "MANUAL-ONLY PROFILE. ONE-LAUNCH APPLY IS NOT SUPPORTED." in src
assert "B_COMMUNITY_MANUAL" in src
assert 'profile && strcmp(profile->trust, "B_COMMUNITY") != 0' in src
assert "WAIT FOR ONLINE DATABASE SYNC TO FINISH BEFORE STARTING." in src
assert "WAIT FOR ONLINE DATABASE OR GAME ACTION TO FINISH." in src
assert "WAIT FOR ONLINE DATABASE SYNC TO FINISH BEFORE STARTING." in src
assert "find_mounted_game(game, &mounted)" in src
assert "if (g_catalog_sync_running || g_catalog_sync_done)" in src
assert "(!candidate->app_ver[0] ||" in src
online_ui = section("static void draw_online_database(void)", "static void draw_scan(void)")
assert "UP/DOWN" not in online_ui
assert "D-PAD SELECT" in online_ui
assert "L1/R1 PREV/NEXT" in online_ui
assert "%.54s" in online_ui
assert "CATEGORY: %.24s" in online_ui
online_input = section("} else if (g_screen == SCREEN_ONLINE_DB) {", "} else if (g_screen == SCREEN_SETTINGS) {")
assert "up && !old_up" in online_input
assert "down && !old_down" in online_input
assert "open_online_search_keyboard" in online_input
assert "ONLINE_FILTER_MODE_COUNT" in online_input
assert "feature/online-graphics-database" in src

online_source = source_path.with_name("online_catalog.c").read_text(encoding="utf-8")
assert "CURLOPT_SSL_VERIFYPEER, 1L" in online_source
assert "CURLOPT_SSL_VERIFYHOST, 2L" in online_source
assert "CURLOPT_CAINFO" in online_source
assert "write_download_data" in online_source
assert "ps3scale_online_download_limited" in online_source

catalog_source = source_path.parent.parent / "core" / "catalog.c"
catalog_src = catalog_source.read_text(encoding="utf-8")
assert "valid_title_id" in catalog_src
assert "valid_app_version" in catalog_src
assert "valid_patch_list" in catalog_src
assert "Duplicate exact identity is ambiguous" in catalog_src

profile_source = source_path.with_name("profile_apply.c").read_text(encoding="utf-8")
assert "PS3SCALE_SCRIPT_HEADER_V1" in profile_source
assert "PS3SCALE_SCRIPT_HEADER_V2" in profile_source
assert "TITLE_ID=%s APP_VER=%s PATCHES=%s" in profile_source
assert "/patch.ps3?addr=" in profile_source
assert "&stop=%08lX&step=%lu&find=%s&rep=1&val=%s" in profile_source
assert "#define PS3SCALE_PROFILE_SEARCH_WINDOW 0x20ul" in profile_source
assert "#define PS3SCALE_PROFILE_SEARCH_STEP 64ul" in profile_source
assert "address + PS3SCALE_PROFILE_SEARCH_WINDOW" in profile_source
assert "address + 1" not in profile_source
assert "strcmp(actual_script, expected_script) == 0" in profile_source
assert '"del %s\\n\\n"' in profile_source
assert "No success notification is shown" in profile_source
assert "profile->patch_count != 1" in profile_source
assert "popup" not in profile_source
assert "beep" not in profile_source

webman_header = source_path.parent.parent.parent / "include" / "ps3scale" / "webman.h"
assert "ps3scale_webman_ps3mapi_available" in webman_header.read_text(encoding="utf-8")

webman_source = source_path.with_name("webman_control.c").read_text(encoding="utf-8")
assert "connect_with_timeout" in webman_source
assert "wait_for_readable" in webman_source
assert "select(sock + 1" in webman_source
assert "ps3scale_webman_build_start_path" in webman_source
assert "sleep(1)" not in webman_source

webman_command_source = source_path.parent.parent / "core" / "webman_command.c"
webman_command_src = webman_command_source.read_text(encoding="utf-8")
assert '"/play.ps3?%s"' in webman_command_src
assert '"/play.ps3%s"' in webman_command_src
assert "encode_game_path" in webman_command_src

print("PS3 SCALE UI contract tests: PASS")


# Online support submission contract.
assert "PS3SCALE_SUPPORT_URL" in src
assert "ps3scale_online_post_json" in src
assert "start_support_send();" in src
assert "support_submit_worker" in src
assert "service_support_send();" in main
assert "LOCAL SAVE OK; ONLINE OFF." in src
assert "PS3SCALE_SUPPORT_OUTBOX_PATH" in src
support_form = section("static void draw_support_form(void)", "static int support_json_escape")
assert "Submit Report" in support_form
assert "Save locally; send online if the service is configured" in support_form

online_source = source_path.with_name("online_catalog.c").read_text(encoding="utf-8")
assert "int ps3scale_online_post_json(" in online_source
assert "CURLOPT_POSTFIELDS" in online_source
assert "CURLOPT_SSL_VERIFYPEER" in online_source
assert "CURLOPT_SSL_VERIFYHOST" in online_source
assert "CURLPROTO_HTTPS" in online_source

repo_root = source_path.parents[3]
support_function = (repo_root / "supabase/functions/ps3scale-support/index.ts").read_text(encoding="utf-8")
support_sql = (repo_root / "supabase/migrations/202611100001_ps3scale_support.sql").read_text(encoding="utf-8")
support_config = (repo_root / "supabase/config.toml").read_text(encoding="utf-8")
assert "SUPABASE_SERVICE_ROLE_KEY" in support_function
assert "PS3SCALE_GITHUB_TOKEN" in support_function
assert "SUPABASE_SERVICE_ROLE_KEY" not in src
assert "PS3SCALE_GITHUB_TOKEN" not in src
assert "verify_jwt = false" in support_config
assert "enable row level security" in support_sql.lower()
assert "grant all on table public.ps3scale_support_reports to service_role" in support_sql

# XMB and scene design contract.
assert "draw_scene_background();" in draw_ui_impl
assert "daylight" in src and "night" in src
assert "tiny3d_AlphaTest(1, 0x10, TINY3D_ALPHA_FUNC_GEQUAL);" in draw_ui_impl
assert "TINY3D_CLEAR_ZBUFFER" in draw_ui_impl
assert "draw_app_icon_preview" in src
assert "XMB APP ICON" in src
assert "draw_stair_tread(4.8f, 0.28f, 2.45f, i);" in src
assert "g_frame_counter % 21600ul" in src
assert "draw_home_card" not in src  # home cards are rendered in the main menu loop
assert "PS3" in home and "SCALE" in home and "COMPANION TOOLS" in home
assert '"Detect PS3 & PS2 Titles"' in home and '"Community Updates"' in home
assert "draw_ps_symbol" in home
project_root = source_path.parents[2]
makefile = (project_root / "Makefile").read_text(encoding="utf-8")
assert "scripts/generate_xmb_assets.py" in makefile
assert "PIC1.PNG" in makefile and "ICON0.PNG" in makefile and "PIC0.PNG" in makefile
xmb_generator = (project_root / "scripts/generate_xmb_assets.py").read_text(encoding="utf-8")
assert '("PIC1.PNG",(1280,720))' in xmb_generator
assert '("ICON0.PNG",(320,176))' in xmb_generator
assert "import_music1_from_usb" in src
assert '"/dev_usb000/music1.mp3"' in src
assert '"/dev_usb003/PS3SCALE/music1.mp3"' in src
assert '"/dev_usb000/BIOS - Zorrovian [Official Visualiser] (1).mp3"' in src
assert '"/dev_usb000/BIOS - Zorrovian [Official Visualiser] (1)(2).mp3"' in src
assert "rename(temp_path, PS3SCALE_MUSIC1_PATH)" in src
assert "MUSIC 1 IMPORTED FROM USB" in settings
sfo = (project_root / "sfo.xml").read_text(encoding="utf-8")
assert '<value name="APP_VER" type="string">00.21</value>' in sfo
assert '<value name="VERSION" type="string">00.21</value>' in sfo

print("PS3 SCALE UI contract tests: PASS")
