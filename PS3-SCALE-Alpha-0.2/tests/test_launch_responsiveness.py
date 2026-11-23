#!/usr/bin/env python3
"""Regression checks for PS3 SCALE game-launch responsiveness and stack safety."""
import pathlib
import sys

src = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")


def section(start, end):
    a = src.index(start)
    b = src.index(end, a + len(start))
    return src[a:b]


# A 256-entry ps3scale_game_list_t is roughly 300 KiB. It must stay in static
# storage, never on the PPU main stack or a 0x8000-byte worker stack.
assert "static ps3scale_game_list_t g_validation_scan;" in src
mounted_scan = section("static int find_mounted_game(const ps3scale_game_t *selected, ps3scale_game_t *mounted)\n{", "static int find_installed_game(const ps3scale_game_t *selected)\n{")
installed_scan = section("static int find_installed_game(const ps3scale_game_t *selected)\n{", "static void game_launch_worker(void *arg)\n{")
assert "ps3scale_game_list_t " not in mounted_scan
assert "ps3scale_game_list_t " not in installed_scan
assert "ps3scale_game_list_init(&g_validation_scan);" in mounted_scan
assert "ps3scale_game_list_init(&g_validation_scan);" in installed_scan
assert "&g_validation_scan" in mounted_scan
assert "&g_validation_scan" in installed_scan

# Exact game identity checks run on the profile worker, not in controller input.
profile_worker = section("static void profile_arm_worker(void *arg)\n{", "static void service_profile_arm(void)\n{")
assert "find_installed_game(&g_profile_arm_game_target)" in profile_worker
assert "find_mounted_game(&g_profile_arm_game_target, &mounted)" in profile_worker
assert "g_profile_arm_result = -102;" in profile_worker

confirm = section("static void confirm_profile_install(void)\n{", "static int find_mounted_game(const ps3scale_game_t *selected, ps3scale_game_t *mounted)\n{")
assert "find_installed_game(" not in confirm
assert "find_mounted_game(" not in confirm
assert "g_profile_arm_game_target = *game;" in confirm

start_game = section("static void start_selected_game(void)\n{", "static void load_app_settings(void)\n{")
assert "find_installed_game(" not in start_game
assert "find_mounted_game(" not in start_game
assert "game_launch_worker" in start_game
assert "force_unmodified = g_force_unmodified_launch" in start_game
assert "if (!is_ps2_iso && !force_unmodified)" in start_game
assert "PROFILE NOT APPLIED. STARTING GAME UNMODIFIED." in start_game

# If profile arming fails for a recoverable reason, explicit CROSS/START
# still launches the selected title unmodified when webMAN can handle it.
profile_service = section("static void service_profile_arm(void)\n{", "static void confirm_profile_install(void)\n{")
assert "result == -101 || result == -102" in profile_service
assert "result == -1 || result == -4 || result == -5" in profile_service
assert "selected_game_matches_auto_launch_target()" in profile_service
assert "g_force_unmodified_launch = 1;" in profile_service
assert "cleanup_result = ps3scale_profile_script_remove(" in profile_service
assert "if (cleanup_result == -2)" in profile_service
assert "USER WEBMAN SCRIPT EXISTS OR CANNOT BE VERIFIED. LAUNCH CANCELLED." in profile_service
assert "GAME SELECTION CHANGED. NO GAME WAS STARTED." in profile_service

# A pre-armed profile is revalidated on the launch worker before PS3MAPI or
# webMAN is called. A mismatch removes only PS3 SCALE's own pending script.
launch_worker = section("static void game_launch_worker(void *arg)\n{", "static void service_game_launch(void)\n{")
assert "find_installed_game(&g_game_launch_target)" in launch_worker
assert "find_mounted_game(&g_game_launch_target, &mounted)" in launch_worker
assert "PS3SCALE_GAME_LAUNCH_ERROR_IDENTITY" in launch_worker
assert "cleanup_result = ps3scale_profile_script_remove(" in launch_worker
assert "if (cleanup_result == -2)" in launch_worker
assert "PS3SCALE_GAME_LAUNCH_ERROR_SCRIPT" in launch_worker
assert launch_worker.index("identity_valid") < launch_worker.index("ps3scale_webman_start_game")

service_launch = section("static void service_game_launch(void)\\n{", "static void start_selected_game(void)\\n{")
assert "selected_game_matches_game_launch_target()" in service_launch
assert "g_force_unmodified_launch = 1;" in service_launch
assert "PS3SCALE_GAME_LAUNCH_ERROR_SCRIPT" in service_launch
assert "PROFILE SCRIPT COULD NOT BE SAFELY REMOVED. GAME NOT STARTED." in service_launch

# Missing or oversized screenshots must not reach the PNG decoder.
preview = section("static void update_selected_preview(void)\n{", "static void draw_details(void)\n{")
assert "image_file_is_safe(before_path, PS3SCALE_PREVIEW_MAX_BYTES)" in preview
assert "image_file_is_safe(after_path, PS3SCALE_PREVIEW_MAX_BYTES)" in preview

print("PS3 SCALE launch responsiveness and stack-safety tests: PASS")