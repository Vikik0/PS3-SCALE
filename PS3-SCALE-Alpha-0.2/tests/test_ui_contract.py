#!/usr/bin/env python3
"""Static UI contract checks for PS3 SCALE's PS3-only UI."""
import math
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
              "Restore Default Settings", "Language", "Music Playlist",
              "Track Fade Duration"):
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
assert "music_fade_ms=%d" in src
assert "settings_version=2" in src
assert "cycle_music_fade_ms" in src
assert "g_settings_selected == 8" in main
assert "MUSIC 1: BIOS - ZORROVIAN (USB IMPORT)" in settings
assert "draw_ps_symbol(405.0f, 91.0f" in src
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
home_animation = section("static void draw_home_animated_card(",
                           "static void draw_home(void)")
home_animation_update = section("static void update_home_card_animations(void)",
                                "static void draw_home(void)")
assert "scale = 1.0f + focus * 0.035f;" in home_animation
assert "draw_rounded_outline" in home_animation
assert "ring_alpha" in home_animation
assert "ui_blend_color(0x5e718f35, 0x665b8f50, focus)" in home_animation
assert "ui_blend_color(0xfaf8ffe8, 0xf4efffe8, focus)" in home_animation
assert "scene_blend(" not in home_animation
ui_color_blend = section("static u32 ui_blend_color(", "static void draw_scene_background(void)")
assert "scene_blend(a, b, t) & 0xffffff00u" in ui_color_blend
assert "alpha_a" in ui_color_blend and "alpha_b" in ui_color_blend
assert "g_home_card_focus[i] += difference * 0.24f;" in home_animation_update
assert "if (!g_animation_enabled)" in home_animation_update
assert "update_home_card_animations();" in main
assert "FW %s" in home and "PS3 %s" in home
assert "g_hardware_info_valid" in home
assert "ps3scale_hardware_family_name(g_hardware_info.family)" in home
assert "pulse = g_animation_enabled ?" in home_animation
scene_background = section("static void draw_scene_background(void)",
                           "static void scene_emit_line_segment")
assert "if (!g_animation_enabled)" in scene_background
assert "phase = 0.0f;" in scene_background
splash_static = section("static void draw_splash(void)", "static void draw_boot(void)")
assert "progress = !g_animation_enabled ? 1.0f" in splash_static
assert "if (!g_animation_enabled)" in splash_static
assert "CFW 4.92" not in home
assert "PS3 SLIM (CECH-25xx)" not in home
assert "SCREEN_HELP" in src and "SCREEN_SUPPORT_FORM" in src
help_ui = section("static void draw_help(void)", "static void draw_about(void)")
assert "Report an Issue" in help_ui and "Send Feedback" in help_ui
assert "A quick guide to the main PS3 SCALE features" in help_ui
assert "PS3SCALE_SUPPORT_OUTBOX_PATH" in src
assert "SAVED TO LOCAL OUTBOX" in src
assert "open_support_keyboard(0)" in main
assert "open_support_keyboard(1)" in main
assert "ya2d_loadPNGfromFile(PS3SCALE_APOLLO_LOGO_PATH)" in main
assert "ya2d_freeTexture(g_apollo_texture)" in main

# Read-only hardware detection reports firmware and the Platform ID without console IDs.
assert "ps3scale_hardware_detect(&g_hardware_info)" in main
about = section("static void draw_about(void)", "static void draw_ui(void)")
for token in ("Hardware Information", "Model", "Console type",
              "Firmware", "Platform ID", "Hardware information is unavailable."):
    assert token in about
hardware_detect_path = source_path.with_name("hardware_detect.c")
hardware_detect_src = hardware_detect_path.read_text(encoding="utf-8")
assert "lv2syscall1(387" in hardware_detect_src
assert "lv2syscall1(985" in hardware_detect_src
assert "ps3scale_hardware_info_parse" in hardware_detect_src
assert "ps3scale_console_type_name" in about
assert "IDPS" not in hardware_detect_src and "PSID" not in hardware_detect_src

# The staircase is an endless camera-relative scene, not a reversing dolly.
ambient = section("static void draw_home_ambient(void)", "static void set_settings_message")
assert "PS3SCALE_STAIR_STEPS 32" in src
assert "PS3SCALE_STAIR_LOOP_FRAMES 21600ul" in src
assert "scene_camera_space_point" in src
assert "scene_project_camera_point" in src
assert "scene_push_face" in ambient
assert "draw_scene_faces(faces, face_count);" in ambient
assert "tiny3d_SetPolygon(TINY3D_QUADS);" in src
assert "compare_scene_face_depth" in ambient
assert "qsort(faces, face_count" in ambient
assert "scene_set_camera(0.0f, camera_y, 0.0f," in ambient
assert "float camera_steps = phase / tau * 18.0f;" in ambient
assert "float camera_y = camera_steps * rise_per_tread;" in ambient
assert "center_y = camera_y;" in ambient
assert "((float)i + camera_steps) * angle_step" in ambient
assert "(t + camera_steps) * angle_step" in ambient
assert "camera_step_whole" not in ambient
assert "Inner and outer spiral handrails" in ambient
assert "tiny3d_SetPolygon(TINY3D_LINES);" in ambient
assert "scene_emit_line_segment" in ambient
assert "g_frame_counter % PS3SCALE_STAIR_LOOP_FRAMES" in ambient
assert "angle_step = tau / 18.0f;" in ambient
assert "outer_radius = 14.0f" in ambient
assert "inner_radius = 2.0f" in ambient
assert "camera_points[i].z <= 2.5f" in src
assert "camera_a.z <= 2.5f || camera_b.z <= 2.5f" in src

# Every camera advance presents the identical geometry in the next tread slot.
# The 21,600-frame boundary advances an entire revolution with no camera cut.
def streamed_tread(frame, index):
    step = (frame % 21600) * (18.0 / 21600.0)
    whole = int(step)
    fraction = step - whole
    return ((index + whole) % 18, (index - 15.5 - fraction) * 0.72)

for boundary in (1200, 2400, 21600):
    for index in range(1, 31):
        before = streamed_tread(boundary - 1, index)
        after = streamed_tread(boundary, index - 1)
        assert before[0] == after[0]
        assert abs(before[1] - after[1]) < 0.001

# Pearl material upgrade: shaded 3D tread geometry must stay bounded.
assert "#define PS3SCALE_STAIR_FACE_MAX (PS3SCALE_STAIR_STEPS * 5)" in src
assert "u32 vertex_color[4];" in src
assert "scene_blend(color, 0xf7f4fcff, haze * 0.34f)" in src
assert "tiny3d_VertexColor(faces[i].vertex_color[j]);" in src
assert "if (faces[i].edge == 0)" in src
assert "ps3scale_scene_point_t pearl_lip[4], contact_shadow[4];" in ambient
assert "pearl_lip[0] = top[1];" in ambient
assert "contact_shadow[0] = riser[0];" in ambient
assert "scene_push_face(faces, &face_count, pearl_lip" in ambient
assert "scene_push_face(faces, &face_count, contact_shadow" in ambient
assert "qsort(faces, face_count" in ambient

# Glass cards and settings chips use real rounded geometry with no textures.
card = section("static void draw_soft_card(", "static void draw_text_centered(")
chip = section("static void draw_soft_chip(", "static void draw_page_header(")
assert "static void draw_rounded_rect(" in src
assert "static void draw_rounded_outline(" in src
assert "TINY3D_TRIANGLES" in src
assert "draw_rounded_rect(x, y, w, h, corner," in card
assert "draw_rounded_outline(x, y, w, h, corner," in card
assert "draw_rounded_rect(x, y, w, 24.0f, 7.0f," in chip
assert "draw_rounded_outline(x, y, w, 24.0f, 7.0f," in chip

# Five vector icons and their selected/normal states retain the home navigation.
icons = section("static void draw_menu_icon(", "static void draw_ps_symbol")
assert "static void draw_icon_diamond(" in src
assert "draw_icon_diamond(x + 21.0f" in icons
assert "u32 tile = selected ?" in icons
assert "draw_gear_outline" in icons
assert "draw_circle_outline" in icons
assert "if (index == 0)" in icons and "index == 3" in icons

assert "case 0xffffff00u: rgb = 0x29243f00u;" in src
assert 'sscanf(line, "animation_v2=%d"' in src
assert 'fprintf(fp, "animation_v2=%d\\n"' in src
draw_ui_impl = section("static void draw_ui(void)", "static int compare_game_titles")
assert "if (g_animation_enabled)" in draw_ui_impl
assert "draw_home_ambient();" in draw_ui_impl
assert "tiny3d_AlphaTest(1, 0x10, TINY3D_ALPHA_FUNC_GEQUAL);" in draw_ui_impl
assert "tiny3d_BlendFunc(1," in draw_ui_impl
assert "TINY3D_CLEAR_ZBUFFER" not in draw_ui_impl

# The library/detail UI upgrade must not touch scanner, launch or apply paths.
library_ui = section("static void draw_library_filter_item(", "static void draw_settings(void)")
assert "draw_rounded_rect(x, y, w, 24.0f, 7.0f" in library_ui
assert "draw_rounded_outline(x - 2.0f, y - 2.0f, w + 4.0f" in library_ui
assert "draw_rounded_rect(x + 107.0f, y + 82.0f" in library_ui
assert "game_is_ps3(game) ? \"PS3\" : \"PS2\"" in library_ui
assert "PAGE %u / %u" in library_ui
assert "PS3SCALE_SCAN_GRID_COUNT" in library_ui
assert "MATCHED PROFILE - REVIEW FIRST" in library_ui
assert "MATCHED PS2 CONFIG - REVIEW FIRST" in library_ui
assert "NO MATCHED FILE - PLAY UNMODIFIED" in library_ui
assert "CONCEPT PREVIEW" in library_ui
assert "This is NOT an HD texture replacement." in library_ui
assert "Illustration only; not real game footage." in library_ui
assert "Higher Resolution (profile-dependent)" not in library_ui
assert "Improved Texture Sharpness" not in library_ui
assert "Smoother Performance (game-specific)" not in library_ui
assert "TRIANGLE: APPLY PROFILE" in library_ui
assert "TRIANGLE: DOWNLOAD CONFIG" in library_ui
assert "CROSS: START GAME" in library_ui

# Stage 5: Settings, Help, About and profile toast share the pearl UI language.
settings_ui = section("static void draw_settings(void)", "static void draw_tools(void)")
assert "PREFERENCES" in settings_ui
assert "GROUP AUTO-HIGHLIGHTS" in settings_ui
assert 'draw_soft_chip(28.0f, 136.0f, 132.0f, "General", active_group == 0)' in settings_ui
assert 'draw_soft_chip(28.0f, 184.0f, 132.0f, "Display", active_group == 1)' in settings_ui
assert 'draw_soft_chip(28.0f, 232.0f, 132.0f, "Audio", active_group == 2)' in settings_ui
assert 'draw_soft_chip(28.0f, 280.0f, 132.0f, "System", active_group == 3)' in settings_ui
assert "draw_rounded_rect(198.0f, y - 5.0f, 620.0f, 28.0f" in settings_ui
assert "draw_rounded_outline(198.0f, y - 5.0f, 620.0f, 28.0f" in settings_ui
assert "draw_rounded_rect(748.0f, y, 54.0f, 18.0f, 9.0f" in settings_ui
assert "g_music_volume" in settings_ui and "g_animation_enabled" in settings_ui
assert "g_music_track1_missing" in settings_ui
help_ui = section("static void draw_help(void)", "static void draw_app_icon_preview(float x, float y)")
for token in ("Scan Games", "Select a Game", "Review Match",
              "Save Data Safely", "Start Game", "support@ps3scale.app",
              "Report an Issue", "Send Feedback",
              "never auto-executed"):
    assert token in help_ui
assert "draw_soft_card(x, 112.0f, 154.0f, 202.0f, 0)" in help_ui
assert "draw_rounded_rect(x + 12.0f, 126.0f, 28.0f, 24.0f" in help_ui
about_ui = section("static void draw_about(void)", "static void draw_profile_toast(void)")
for token in ("READ-ONLY SCANNING", "VERSION-MATCHED DATA",
              "NO AUTO-APPLY", "Hardware Information",
              "CONSOLE DETECTED", "NOT AVAILABLE",
              "No universal upscaler or guaranteed 60 FPS is claimed.",
              "draw_app_icon_preview(645.0f, 136.0f)"):
    assert token in about_ui
icon_ui = section("static void draw_app_icon_preview(float x, float y)",
                  "static void draw_about(void)")
assert "draw_rounded_rect(x, y, 164.0f, 190.0f, 18.0f" in icon_ui
assert "PS3" in icon_ui and "SCALE" in icon_ui
toast_ui = section("static void draw_profile_toast(void)", "static void draw_ui(void)")
assert "draw_rounded_rect(568.0f, 16.0f, 264.0f, 48.0f, 10.0f" in toast_ui
assert "ONE-LAUNCH PROFILE ARMED" in toast_ui
settings_input = section("} else if (g_screen == SCREEN_SETTINGS) {",
                         "} else if (g_screen == SCREEN_TOOLS) {")
assert "g_settings_selected = (g_settings_selected + 1) % SETTINGS_COUNT" in settings_input
assert "set_background_music_enabled" in settings_input
assert "g_animation_enabled = !g_animation_enabled" in settings_input
assert "scan_library();" in settings_input
assert "save_app_settings();" in settings_input

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
assert "APPLY: REFERENCE ONLY - NOT INSTALLED" not in src
assert "X: DOWNLOAD TO COMMUNITY_DOWNLOADS" in src
assert "DATA ONLY - NEVER EXECUTED OR AUTO-APPLIED" in src
assert "community_download_worker" in src
assert "start_community_file_download" in src
assert "PS3SCALE_COMMUNITY_INDEX_DOWNLOAD_LIMIT" in src
assert "community_downloads" in src
assert "community_component_is_safe" in src
assert "community_extension_is" in src
assert "memchr(header, 0, got)" in src
assert "CROSS OPEN/DOWNLOAD" in src
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
# Online DB stage 4: page/selection must stay in sync for >8 results.
assert "#define PS3SCALE_ONLINE_PAGE_SIZE 8u" in src
assert "page_start = ((size_t)g_online_selected / page_size) * page_size;" in online_ui
assert "i = page_start;" in online_ui
assert "visible_count < page_size;" in online_ui
assert "RESULTS %u  |  PAGE %u/%u" in online_ui
assert "SEARCH RESULT: NO LOCAL GAME MATCH" in online_ui
assert "MATCH: ID OR TITLE/REGION + VERSION" in online_ui
assert "RESULTS 0  |  NO MATCHES" in online_ui
assert 'kind = "VER OK";' in online_ui
assert "MATCH: REVIEW TITLE ID / VERSION" in online_ui
assert "PROFILE CACHE: LOADED" in online_ui
assert "GITHUB INDEX: LOADED" in online_ui
assert "g_catalog_sync_running" in online_ui
assert "DATA ONLY - NEVER EXECUTED OR AUTO-APPLIED" in online_ui
assert "NO DOWNLOAD: ONLY NCL/TXT UP TO 2 MB" in online_ui
assert "PS2 compatibility is game-specific." in online_ui
assert "Not an HD texture or 1080p upgrade." in online_ui
assert "START: SYNC ONLINE METADATA" in online_ui
assert "draw_rounded_rect(32.0f, y, 334.0f" in online_ui
assert "draw_soft_chip(414.0f, 190.0f" in online_ui
assert "LEFT/RIGHT PAGE" in online_ui
assert "CROSS OPEN/DOWNLOAD" in online_ui
assert 'selected ? "X: GAME DETAILS" : "X: DOWNLOAD DATA"' in online_ui
assert "SELECT: SAVE TO COMMUNITY_DOWNLOADS" in online_ui

def online_page_state(total, selected, page_size=8):
    if total == 0:
        return 0, 0, 0
    assert 0 <= selected < total
    start = selected // page_size * page_size
    return start, min(total, start + page_size), start // page_size + 1

def move_page(total, selected, direction, page_size=8):
    if not total:
        return 0
    count = (total + page_size - 1) // page_size
    page, row = divmod(selected, page_size)
    next_page = (page + (1 if direction > 0 else count - 1)) % count
    return min(total - 1, next_page * page_size + row)

for total in [0, 1, 7, 8, 9, 15, 16, 17, 61]:
    for index in range(total):
        lo, hi, page = online_page_state(total, index)
        assert 0 <= lo <= index < hi <= total
        assert hi - lo <= 8
        assert 1 <= page <= (total + 7) // 8
        for direction in [-1, 1]:
            nxt = move_page(total, index, direction)
            lo2, hi2, _ = online_page_state(total, nxt)
            assert lo2 <= nxt < hi2
    if total == 0:
        assert online_page_state(0, 0) == (0, 0, 0)
        assert move_page(0, 0, 1) == 0

online_input = section("} else if (g_screen == SCREEN_ONLINE_DB) {", "} else if (g_screen == SCREEN_SETTINGS) {")
assert "up && !old_up" in online_input
assert "down && !old_down" in online_input
# GRAPHICS-only result filters must not include PS2 compatibility configs.
online_results_source = section("static void rebuild_online_results(void)", "static const ps3scale_game_t *online_result_at(")
assert "if (!graphics_only && g_online_query[0] && g_ps2_config_db.items)" in online_results_source
assert "PS3SCALE_ONLINE_PAGE_SIZE" in online_input
assert "(left && !old_left)" in online_input
assert "(right && !old_right)" in online_input
assert "next_selection >= matches" in online_input
assert "if (select && !old_select && matches)" in online_input
assert "start_community_file_download(community)" in online_input
assert "open_selected_online_match();" in online_input
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
assert "TINY3D_CLEAR_ZBUFFER" not in draw_ui_impl
assert "draw_home_ambient();" in draw_ui_impl
assert "draw_app_icon_preview" in src
assert "XMB APP ICON" in src
assert "scene_camera_space_point" in src
assert "PS3SCALE_STAIR_STEPS 32" in src
assert "g_frame_counter % PS3SCALE_STAIR_LOOP_FRAMES" in src
assert "draw_home_card" not in src  # home cards are rendered in the main menu loop
assert "PS3" in home and "SCALE" in home and "COMPANION TOOLS" in home
assert '"PS3 / PS2 Library"' in home and '"Game Profiles"' in home
assert "draw_ps_symbol" in home
project_root = source_path.parents[2]
makefile = (project_root / "Makefile").read_text(encoding="utf-8")
assert "scripts/generate_xmb_assets.py" in makefile
assert "PIC1.PNG" in makefile and "ICON0.PNG" in makefile and "PIC0.PNG" in makefile
xmb_generator = (project_root / "scripts/generate_xmb_assets.py").read_text(encoding="utf-8")
assert '("PIC1.PNG",(1280,720))' in xmb_generator
assert '("ICON0.PNG",(320,176))' in xmb_generator
assert "def spiral_staircase" in xmb_generator
assert "spiral_staircase(bg,28" in xmb_generator
assert "import_music1_from_usb" in src
assert '"/dev_usb000/music1.mp3"' in src
assert '"/dev_usb003/PS3SCALE/music1.mp3"' in src
assert '"/dev_usb000/BIOS - Zorrovian [Official Visualiser] (1).mp3"' in src
assert '"/dev_usb000/BIOS - Zorrovian [Official Visualiser] (1)(2).mp3"' in src
assert "rename(temp_path, PS3SCALE_MUSIC1_PATH)" in src
assert "MUSIC 1 IMPORTED FROM USB" in settings
sfo = (project_root / "sfo.xml").read_text(encoding="utf-8")
assert '<value name="APP_VER" type="string">00.22</value>' in sfo
assert '<value name="VERSION" type="string">00.22</value>' in sfo

# Stage 6: cohesive splash, first-run language, boot and global transitions.
assert "#define PS3SCALE_TRANSITION_FRAMES 16u" in src
assert "g_transition_screen = SCREEN_SPLASH" in src
assert "g_transition_frames = PS3SCALE_TRANSITION_FRAMES" in src

language_ui = section("static void draw_language_selection(void)",
                      "static void draw_splash(void)")
for token in ("CHOOSE LANGUAGE", "اختر اللغة", "Elige el idioma",
              "ps3scale_locale_language_name(i)",
              "draw_rounded_rect(128.0f, y, 598.0f, 40.0f"):
    assert token in language_ui

splash_ui = section("static void draw_splash(void)", "static void draw_boot(void)")
for token in ("PS3 SCALE / CONSOLE MANAGEMENT", "PS3SCALE_APP_VERSION",
              "PS3 & PS2 Enhancement Management", "SPECIAL THANKS",
              "ARTEMIS", "APOLLO SAVE", "LOADING PS3 SCALE",
              "PRESS CROSS OR START TO SKIP", "draw_ps_symbol",
              "g_apollo_texture", "progress_text"):
    assert token in splash_ui
assert "draw_rounded_rect(98.0f, 48.0f, 646.0f, 410.0f" in splash_ui
assert "draw_rounded_rect(210.0f, 402.0f, 434.0f, 11.0f" in splash_ui
assert "frame >= 179 ? 1.0f" in splash_ui

boot_ui = section("static void draw_boot(void)", "static void draw_keyboard(void)")
for token in ("Starting your game library", "SCANNING LOCAL GAME LIBRARY",
              "APP FILE CHECK: OK", "APP FILE CHECK: VERIFY INSTALL",
              "draw_rounded_rect(92.0f, 118.0f, 670.0f, 260.0f",
              "g_boot_stage == 2"):
    assert token in boot_ui

transition_ui = section("static void draw_screen_transition(void)",
                        "static void draw_ui(void)")
for token in ("PS3SCALE_TRANSITION_FRAMES", "sweep_x",
              "color_with_alpha(0xfaf8ffff, alpha)",
              "g_transition_frames++",
              "if (!g_animation_enabled)",
              "g_transition_frames = PS3SCALE_TRANSITION_FRAMES;"):
    assert token in transition_ui
assert "if (g_screen != g_transition_screen)" in draw_ui_impl
assert "g_transition_screen = g_screen;" in draw_ui_impl
assert "g_transition_frames = 0;" in draw_ui_impl
assert "draw_screen_transition();" in draw_ui_impl
assert draw_ui_impl.index("draw_screen_transition();") < draw_ui_impl.index("tiny3d_Flip();")
assert "(cross && !old_cross) || (start && !old_start)" in main
assert "++g_splash_frames >= 180" in main
assert "g_screen = g_splash_next_screen;" in main

# Warning cleanup and Node 24 action runtime contracts.
assert "static void format_hex" not in src
online_catalog_source = (source_path.parent / "online_catalog.c").read_text(encoding="utf-8")
assert online_catalog_source.index("#include <sys/time.h>") < online_catalog_source.index("#include <curl/curl.h>")

repo_root = source_path.parents[3]
build_workflow = (repo_root / ".github/workflows/build-ps3.yml").read_text(encoding="utf-8")
index_workflow = (repo_root / ".github/workflows/update-community-index.yml").read_text(encoding="utf-8")
assert "actions/checkout@v7" in build_workflow
assert "actions/upload-artifact@v7" in build_workflow
assert "actions/checkout@v7" in index_workflow
assert "actions/setup-python@v7" in index_workflow
assert "actions/checkout@v4" not in build_workflow + index_workflow
assert "actions/upload-artifact@v4" not in build_workflow
assert "actions/setup-python@v5" not in index_workflow

# Keep the visible app, support payload and local report on one version.
assert '#define PS3SCALE_APP_VERSION "ALPHA 0.22"' in src
assert "ALPHA 0.21" not in src
assert "CFW 4.92" not in src
assert "PS3SCALE_APP_VERSION" in section("static void draw_splash(void)",
                                         "static void draw_boot(void)")
support_json = section("static int build_support_json(void)",
                       "static void support_submit_worker(void *arg)")
assert r'\"app_version\":\"%s\"' in support_json
assert "PS3SCALE_APP_VERSION" in support_json
assert 'fprintf(fp, "APP VERSION: %s\\n", PS3SCALE_APP_VERSION);' in src
about_ui = section("static void draw_about(void)",
                   "static void draw_profile_toast(void)")
assert "version_line" in about_ui
assert "ps3scale_hardware_family_name(g_hardware_info.family)" in about_ui
# Download workers are joined: never create a detached thread and then join it.
ps2_worker = section("static void ps2_config_download_worker(void *arg)",
                     "static void start_ps2_config_download(void)")
ps2_start = section("static void start_ps2_config_download(void)",
                    "static int community_extension_is")
community_start = section("static void start_community_file_download(",
                          "static void service_community_download(void)")
assert "THREAD_JOINABLE" in ps2_start
assert "THREAD_JOINABLE" in community_start
assert "unsigned char data[" not in ps2_worker
assert "malloc((size_t)file_size)" in ps2_worker
assert "free(data);" in ps2_worker
assert "file_size <= 0" in ps2_worker

# All libcurl/network-module work is serialized across background workers.
catalog_start = section("static void start_catalog_sync_worker(int force)",
                        "static void service_catalog_sync(void)")
support_start = section("static void start_support_send(void)",
                        "static void service_support_send(void)")
profile_start = section("static void confirm_profile_install(void)",
                        "static int find_mounted_game(")
game_start = section("static void start_selected_game(void)",
                     "static void load_app_settings(void)")
ps2_start = section("static void start_ps2_config_download(void)",
                    "static int community_extension_is")
community_start = section("static void start_community_file_download(",
                          "static void service_community_download(void)")
for network_start in (catalog_start, support_start, profile_start, game_start,
                      ps2_start, community_start):
    assert "g_network_task_running" in network_start
    assert "g_network_task_running = 1;" in network_start
catalog_worker = section("static void catalog_sync_worker(void *arg)",
                            "static void start_catalog_sync_worker(int force)")
support_worker = section("static void support_submit_worker(void *arg)",
                         "static void start_support_send(void)")
profile_worker = section("static void profile_arm_worker(void *arg)",
                         "static void service_profile_arm(void)")
game_worker = section("static void game_launch_worker(void *arg)",
                      "static void service_game_launch(void)")
community_worker = section("static void community_download_worker(void *arg)",
                           "static void start_community_file_download(")
for network_worker in (catalog_worker, support_worker, profile_worker,
                       game_worker, ps2_worker, community_worker):
    assert "g_network_task_running = 0;" in network_worker
assert "static volatile int g_network_task_running = 0;" in src

# Audio callback and UI share these two live settings while music is playing.
assert "static volatile int g_music_mode" in src
assert "static volatile int g_music_fade_ms" in src

# The spiral staircase camera must move continuously and close its loop.
staircase = section("static void draw_home_ambient(void)",
                    "static void set_settings_message(const char *message)")
assert "float camera_steps = phase / tau * 18.0f;" in staircase
assert "float camera_y = camera_steps * rise_per_tread;" in staircase
assert "center_y = camera_y;" in staircase
assert "((float)i + camera_steps) * angle_step" in staircase
assert "(t + camera_steps) * angle_step" in staircase
assert "camera_step_whole" not in staircase
loop_frames = 21600
def staircase_phase(frame):
    phase = (frame % loop_frames) / float(loop_frames) * (2.0 * math.pi)
    steps = phase / (2.0 * math.pi) * 18.0
    angle = steps * (2.0 * math.pi / 18.0)
    return math.cos(angle), math.sin(angle)
loop_start = staircase_phase(0)
loop_end = staircase_phase(loop_frames - 1)
assert math.hypot(loop_end[0] - loop_start[0],
                  loop_end[1] - loop_start[1]) < 0.006
# Pearl surfaces keep readable ink and lavender focus states.
chip_ui = section("static void draw_soft_chip(", "static void draw_page_header(")
button_ui = section("static void draw_soft_button(", "static void draw_home_animated_card(")
assert "selected ? 0x875be8ff : 0x514b69ff" in chip_ui
assert "selected ? 0x875be8ff : 0x29243fff" in button_ui
assert "0xcaff39ff" not in chip_ui + button_ui
assert "0xffffffff" not in chip_ui + button_ui
# Search and support text entry use the same pearl / lavender visual language.
keyboard_ui = section("static void draw_keyboard(void)",
                      "/* Light-weight rounded panels:")
for token in ("SEARCH ONLINE DATABASE", "draw_rounded_rect(40.0f, 112.0f",
              "0xeee7ffff", "0x9568edff", "0x29243fff",
              "ps3scale_locale_keyboard_key(g_language, i)",
              "ARROWS MOVE  |  CROSS SELECT  |  CIRCLE CANCEL"):
    assert token in keyboard_ui
assert "0xcaff39ff" not in keyboard_ui
assert "0x0d1722ff" not in keyboard_ui
support_form_ui = section("static void draw_support_form(void)",
                          "static int support_json_escape")
assert "selected ? 0x875be8ff : 0x514b69ff" in support_form_ui
assert "g_support_title[0] ? 0x29243fff : 0x77718fff" in support_form_ui
assert "g_support_details[0] ? 0x29243fff : 0x77718fff" in support_form_ui
assert "0xcaff39ff" not in support_form_ui
assert "0xffffffff" not in support_form_ui
# Dark ink remains readable on the light pearl panels.
home_ui = section("static void draw_home(void)",
                  "static void draw_online_database(void)")
details_ui = section("static void draw_details(void)",
                     "static void draw_settings(void)")
tools_ui = section("static void draw_tools(void)",
                   "static void draw_help(void)")
keyboard_ui = section("static void draw_keyboard(void)",
                      "/* Light-weight rounded panels:")
assert 'draw_text(70.0f, 18.0f, 0x29243fff, "PS3")' in home_ui
assert "0xffffffff" not in details_ui
assert "0xffffffff" not in tools_ui
assert "0xffffffff" not in keyboard_ui
assert 'draw_text(48.0f, 120.0f, 0x29243fff, "APOLLO SAVE TOOL")' in tools_ui
print("PS3 SCALE UI contract tests: PASS")
