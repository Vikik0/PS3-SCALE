#!/usr/bin/env python3
"""Static UI contract checks for the PS3-only main.c source."""
import pathlib
import sys

source_path = pathlib.Path(sys.argv[1])
src = source_path.read_text(encoding="utf-8")

def section(start, end):
    a = src.index(start)
    b = src.index(end, a + len(start))
    return src[a:b]

# Native OSK lifecycle: the old implementation unloaded without collecting input.
osk = section("static void osk_sysutil_callback", "static void draw_text")
for token in (
    "SYSUTIL_OSK_INPUT_ENTERED",
    "SYSUTIL_OSK_DONE",
    "SYSUTIL_OSK_INPUT_CANCELED",
    "SYSUTIL_OSK_UNLOADED",
    "oskGetInputText(&g_osk_result)",
    "SYSUTIL_EVENT_SLOT1",
):
    assert token in osk, f"Missing OSK handling: {token}"

# Search must remain wired to SELECT on the Scan Games screen.
main = src[src.index("int main(int argc, char *argv[])"):]
assert "if (select && !old_select)" in main
assert "(void)open_search_keyboard();" in main

# Layout contract: compact font, independent metadata column and lower cover.
scan = section("static void draw_scan(void)", "static void draw_details(void)")
assert "SetFontSize(10, 16)" in scan
assert "draw_text(530, 176" in scan
assert "ya2d_drawTextureEx(g_cover_texture, 690.0f, 286.0f" in scan
assert "draw_text(28, 470" in scan

# Settings are persistent and expose all intended actions.
settings = section("static void draw_settings(void)", "static void draw_tools(void)")
assert "BACKGROUND MUSIC" in settings
assert "MUSIC VOLUME" in settings
assert "HOME ANIMATION" in settings
assert "CHECK FOR USB UPDATE" in settings
assert "RESCAN GAME LIBRARY" in settings
assert "RESTORE DEFAULT SETTINGS" in settings
assert "PS3SCALE_SETTINGS_PATH" in src
assert "settings.cfg" in src

# Home motion is deliberately slow and low-complexity.
assert "g_frame_counter * 0.0024f" in src
assert "draw_circle_outline" in section("static void draw_home_ambient", "static void set_settings_message")
assert "g_animation_enabled" in src

# Library ordering and companion-tool scope are explicit.
assert "qsort(g_games.items" in src
assert "SCREEN_TOOLS" in src
assert "APOLLO SAVE TOOL" in src
assert "ARTEMIS PS3" in src
assert "webMAN MOD" in src

print("PS3 SCALE UI contract tests: PASS")
