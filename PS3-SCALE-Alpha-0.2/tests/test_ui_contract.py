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

# Layout contract: compact font, independent list/metadata/cover columns.
scan = section("static void draw_scan(void)", "static void draw_details(void)")
assert "set_ui_font_size(9, 14)" in scan
assert "draw_text(452, 178" in scan
assert "ya2d_drawTextureEx(g_cover_texture, 680.0f, 282.0f" in scan
assert "SELECT/START: SEARCH" in scan

# Settings retain the expected maintenance/audio actions and add language selection.
settings = section("static void draw_settings(void)", "static void draw_tools(void)")
for token in ("BACKGROUND MUSIC", "MUSIC VOLUME", "HOME ANIMATION",
              "CHECK FOR USB UPDATE", "RESCAN GAME LIBRARY",
              "RESTORE DEFAULT SETTINGS", "LANGUAGE"):
    assert token in settings, f"Missing settings entry: {token}"
assert "COPY MUSIC TO INTERNAL HDD" not in settings
assert "copy_music_to_internal" not in src
assert "AUDIO ENGINE:" not in settings
assert "g_music_path" not in src
assert "mpg123_open(g_mpg123, PS3SCALE_MUSIC_PATH)" in src
assert "START GAME" in src
assert "start_selected_game()" in main
assert "ps3scale_webman_start_mounted_game()" in src
assert "APP VERSION:" in src
assert "g_music_enabled && !g_music_playing" in main
assert "BACKGROUND MUSIC STARTED." in main
assert "PS3SCALE_SETTINGS_PATH" in src
assert "settings.cfg" in src

# Read-only hardware detection reports firmware and the Platform ID without console IDs.
assert "ps3scale_hardware_detect(&g_hardware_info)" in main
about = section("static void draw_about(void)", "static void draw_ui(void)")
for token in ("HARDWARE INFORMATION", "DETECTED MODEL:", "CONSOLE TYPE:",
              "FIRMWARE:", "PLATFORM ID:", "HARDWARE INFO UNAVAILABLE."):
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
assert "g_frame_counter * 0.0012f" in ambient
draw_ui_impl = section("static void draw_ui(void)", "static int compare_game_titles")
assert "if (g_animation_enabled)" in draw_ui_impl
assert "draw_home_ambient();" in draw_ui_impl

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
assert "ONLINE GRAPHICS DATABASE" in src
assert "SCREEN_ONLINE_DB" in src
assert "PS3SCALE_CATALOG_URL" in src
assert "start_catalog_sync_worker(0)" in src
assert "start_catalog_sync_worker(1)" in main
assert "ps3scale_catalog_match_game" in src
assert "EXACT ONLINE PROFILE FOUND" in src
assert "ONE-LAUNCH PROFILE ARMED. START THE MOUNTED GAME." in src
assert "ONE-LAUNCH APPLY: AVAILABLE AFTER CONFIRMATION" in src
assert "STALE PROFILE SCRIPT. REMOVE IT BEFORE STARTING." in src
assert "MULTI-WRITE PROFILE DISABLED FOR AUTO APPLY SAFETY" in src
assert "PROFILE DATA SAVED TO INTERNAL HDD. NOT APPLIED YET." in src
assert "profile_script_state_for_game" in src
assert "profile ? profile->patches : NULL" in src
assert "MOUNT THE EXACT GAME VERSION BEFORE ARMING THIS PROFILE." in src
assert "MANUAL-ONLY PROFILE. ONE-LAUNCH APPLY IS NOT SUPPORTED." in src
assert "B_COMMUNITY_MANUAL" in src
assert "WAIT FOR ONLINE DATABASE SYNC TO FINISH." in src
assert "WAIT FOR ONLINE DATABASE SYNC TO FINISH BEFORE STARTING." in src
assert "find_mounted_game(game, &mounted)" in src
assert "if (g_catalog_sync_running || g_catalog_sync_done)" in src
assert "(!candidate->app_ver[0] ||" in src
assert "feature/online-graphics-database" in src

online_source = source_path.with_name("online_catalog.c").read_text(encoding="utf-8")
assert "CURLOPT_SSL_VERIFYPEER, 1L" in online_source
assert "CURLOPT_SSL_VERIFYHOST, 2L" in online_source
assert "CURLOPT_CAINFO" in online_source
assert "write_catalog_data" in online_source

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

print("PS3 SCALE UI contract tests: PASS")
