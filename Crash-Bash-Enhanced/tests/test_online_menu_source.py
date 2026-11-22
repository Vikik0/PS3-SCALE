#!/usr/bin/env python3
"""Guard the native PS3 lobby's polling and expired-session recovery paths."""

from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
source = (root / "source/online_menu.c").read_text(encoding="utf-8")

assert "#define ROOM_HEARTBEAT_FRAMES 300" in source
assert "#define ROOM_LIST_REFRESH_FRAMES 600" in source
assert "if (++room_list_poll_frames >= ROOM_LIST_REFRESH_FRAMES)" in source
assert "if (screen == SCREEN_ROOM && ++poll_frames >= ROOM_HEARTBEAT_FRAMES)" in source
assert "static int session_is_gone(const char *error)" in source
assert 'strstr(error, "expired")' in source
assert 'strstr(error, "no longer")' in source
assert "static void return_to_rooms(" in source
assert "memset(session, 0, sizeof(*session))" in source
assert "Your room ended or your seat expired." in source

heartbeat = re.search(
    r"if \(screen == SCREEN_ROOM && \+\+poll_frames >= ROOM_HEARTBEAT_FRAMES\)"
    r"(.*?)(?=\n            if \(screen == SCREEN_ROOM\) draw_room)",
    source,
    re.S,
)
assert heartbeat, "Periodic room heartbeat block is missing"
assert heartbeat.group(1).count("cb_online_set_ready(") == 1
assert "cb_online_get_room(" not in heartbeat.group(1), (
    "Heartbeat must return the player list itself; do not issue a duplicate room GET."
)

print("Native lobby polling and expired-session recovery checks passed.")

main_source = (root / "source/main.c").read_text(encoding="utf-8")
online_source = (root / "source/online.c").read_text(encoding="utf-8")
launcher = (root / "source/game_launcher.c").read_text(encoding="utf-8")
assert "BTN_START" in source
assert "cb_launch_game(" in source
assert "ALL PLAYERS READY - PRESS START TO OPEN CRASH BASH" in source
assert "view->player_count >= 2" in online_source
assert "OPEN CRASH BASH ONLINE" in main_source
assert 'cb_launch_game(discs[0].path, 0, "CrashBashPlayer"' in main_source
assert "/dev_hdd0/game/CBON00001/USRDIR/EBOOT.BIN" in launcher
assert "sysProcessExitSpawn2(CB_GAME_EBOOT, arguments" in launcher
assert "TRAINING ARENA" not in main_source
assert "OFFLINE ARENA TEST" not in main_source

disc_source = (root / "source/disc_browser.c").read_text(encoding="utf-8")
launch_args = (root / "source/game_launch_args.c").read_text(encoding="utf-8")
assert "#define SCREEN_DISC_SELECT 3" in source
assert "cb_disc_scan(discs, CB_DISC_MAX, &scanned)" in source
assert "cb_launch_game(discs[disc_selected].path, is_host, netplay_nickname" in source
assert "if ((screen == SCREEN_ROOM || screen == SCREEN_DISC_SELECT)" in source
assert "CB-%.6s-HOST" in source
assert "No CUE files were found." in source
assert '"/dev_hdd0/PSXISO"' in disc_source
assert '"/dev_usb000/PSXISO"' in disc_source
assert "is_host ? \"--host\" : \"--menu\"" in launch_args
assert "CB_GAME_NETPLAY_PORT" in launch_args
assert "host_player_id" in online_source
