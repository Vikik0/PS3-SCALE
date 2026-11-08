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
assert "g_settings_selected == 7" in main

# Startup sequence: local checks and a background library scan with progress.
assert "SCREEN_BOOT" in src
assert "draw_boot" in src
assert "boot_scan_worker" in src
assert "start_boot_scan" in src
assert "SCANNING LOCAL GAME LIBRARY" in src
assert "APP FILE CHECK: OK" in src

# Arabic uses a pinned Noto Sans Arabic UI TTF with FreeType advances;
# ASCII and Spanish extras retain their compact bitmap fonts.
assert "ps3scale_locale_translate" in src
assert "ps3scale_locale_to_glyphs" in src
assert "ps3scale_locale_glyph_font_index" in src
assert "NotoSansArabicUI.ttf" in src
assert "font_bitmap_z.h" in src
assert "arabic_glyph_map.h" not in src  # shaping map is isolated in locale.c
font_loader = section("static int load_extended_font(void)", "static void draw_text")
assert "ps3scale_font8x12" in font_loader
assert "AddFontFromTTF(texture_cursor, 128, 244, 32, 32" in font_loader
assert "g_extended_font_bitmap + spanish_offset" in font_loader
assert "FT_New_Face" in font_loader
assert "FT_Set_Pixel_Sizes" in font_loader
assert "FT_Get_Char_Index" in font_loader
assert "BIT7_FIRST_PIXEL" in font_loader
arabic_callback = section("static void arabic_ttf_glyph_callback", "static void close_arabic_ttf")
assert "0xfe80u + (uint32_t)(chr - 128u)" in arabic_callback
assert "FT_Load_Glyph" in arabic_callback
assert "FT_Render_Glyph" in arabic_callback
assert "FT_PIXEL_MODE_GRAY" in arabic_callback
draw_text_impl = section("static void draw_text", "static int open_background_music")
assert "SetCurrentFont(font_index)" in draw_text_impl
assert "cursor = DrawString(cursor, y, one)" in draw_text_impl
assert "g_ui_font_width * 0.5f" in draw_text_impl
assert "previous_arabic" in draw_text_impl and "next_arabic" in draw_text_impl
assert "ps3scale_locale_is_arabic_glyph" in draw_text_impl
assert "DrawChar(" not in draw_text_impl
assert src.count("SetFontSize(") == 1  # only the tracked UI font-size wrapper may call libfont directly

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
              "RESTORE DEFAULT SETTINGS", "COPY MUSIC TO INTERNAL HDD",
              "LANGUAGE"):
    assert token in settings, f"Missing settings entry: {token}"
assert "copy_music_to_internal" in src
assert "PS3SCALE_SETTINGS_PATH" in src
assert "settings.cfg" in src

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
assert "PLAYBACK NOW USES INTERNAL HDD" in src

print("PS3 SCALE UI contract tests: PASS")
