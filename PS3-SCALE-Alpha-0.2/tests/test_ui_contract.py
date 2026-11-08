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

# Search must be wired to both SELECT and START, and the initial OSK panel
# must be included in the allowed panel mask.
main = src[src.index("int main(int argc, char *argv[])"):]
assert "(select && !old_select) || (start && !old_start)" in main
assert "(void)open_search_keyboard();" in main
osk_open = section("static int open_search_keyboard", "static void draw_text")
assert "OSK_PANEL_TYPE_ALPHABET_FULL_WIDTH" in osk_open
assert "g_osk_params.firstViewPanel = OSK_PANEL_TYPE_ALPHABET_FULL_WIDTH" in osk_open

# Layout contract: smaller font, fixed columns and cover art separated from text.
scan = section("static void draw_scan(void)", "static void draw_details(void)")
assert "SetFontSize(9, 14)" in scan
assert "draw_text(452, 178" in scan
assert "ya2d_drawTextureEx(g_cover_texture, 680.0f, 282.0f" in scan
assert "draw_text(28, 470" in scan
assert "SELECT/START: SEARCH" in scan

# Settings are persistent and expose all intended actions.
settings = section("static void draw_settings(void)", "static void draw_tools(void)")
assert "BACKGROUND MUSIC" in settings
assert "MUSIC VOLUME" in settings
assert "HOME ANIMATION" in settings
assert "CHECK FOR USB UPDATE" in settings
assert "RESCAN GAME LIBRARY" in settings
assert "RESTORE DEFAULT SETTINGS" in settings
assert "COPY MUSIC TO INTERNAL HDD" in settings
assert "copy_music_to_internal" in src
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
assert "refresh_companion_tool_status" in src
assert "g_apollo_installed" in src
assert "g_artemis_installed" in src
assert "g_webman_installed" in src
assert "shutdown_audio_backend();" in main
assert "PLAYBACK NOW USES INTERNAL HDD" in src
assert "MUSIC ALREADY ON HDD; PLAYBACK SWITCHED TO INTERNAL" in src

print("PS3 SCALE UI contract tests: PASS")
