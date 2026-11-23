#!/usr/bin/env python3
"""Guard the native PS3 lobby's polling and expired-session recovery paths."""

from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
source = (root / "source/online_menu.c").read_text(encoding="utf-8")

assert "#define ROOM_HEARTBEAT_FRAMES 300" in source
assert "#define ROOM_LIST_REFRESH_FRAMES 600" in source
assert "if (++room_list_poll_frames >= ROOM_LIST_REFRESH_FRAMES)" in source
assert "if ((screen == SCREEN_ROOM || screen == SCREEN_DISC_SELECT)" in source
assert "static int session_is_gone(const char *error)" in source
assert 'strstr(error, "expired")' in source
assert 'strstr(error, "no longer")' in source
assert "static void return_to_rooms(" in source
assert "memset(session, 0, sizeof(*session))" in source
assert "Your room ended or your seat expired." in source

heartbeat = re.search(
    r"if \(\(screen == SCREEN_ROOM \|\| screen == SCREEN_DISC_SELECT\)\s*"
    r"&&\s*\+\+poll_frames >= ROOM_HEARTBEAT_FRAMES\)"
    r"(.*?)(?=\n            if \(screen == SCREEN_DISC_SELECT && host_disc_waiting|\n            if \(screen == SCREEN_ROOM\) draw_room)",
    source,
    re.S,
)
assert heartbeat, "Periodic room heartbeat block is missing"
assert heartbeat.group(1).count("cb_online_heartbeat(") == 1
assert "cb_online_set_ready(" not in heartbeat.group(1)
assert "cb_online_get_room(" not in heartbeat.group(1), (
    "Heartbeat must return the player list itself; do not issue a duplicate room GET."
)

print("Native lobby polling and expired-session recovery checks passed.")

main_source = (root / "source/main.c").read_text(encoding="utf-8")
online_source = (root / "source/online.c").read_text(encoding="utf-8")
assert "cb_online_heartbeat" in online_source
assert '"set_ready"' in online_source
assert "cb_online_set_ready(&session, next_ready" in source
launcher = (root / "source/game_launcher.c").read_text(encoding="utf-8")
assert "BTN_START" in source
assert "cb_launch_game(" in source
assert "ALL PLAYERS READY - PRESS START TO SELECT THE GAME" in source
assert "if (view->game_started)" in source
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
assert re.search(
    r"cb_launch_game\(discs\[disc_selected\]\.path,\s*1,\s*netplay_nickname",
    source,
), "Host launch must pass the selected disc and host role."
assert re.search(
    r"cb_launch_game\(discs\[disc_selected\]\.path,\s*0,\s*netplay_nickname",
    source,
), "Client launch must pass the selected disc and client role."
assert "if ((screen == SCREEN_ROOM || screen == SCREEN_DISC_SELECT)" in source
assert "CB-%.12s-HOST" in source
assert "No CUE files were found." in source
assert '"/dev_hdd0/PSXISO"' in disc_source
assert '"/dev_usb000/PSXISO"' in disc_source
assert 'argv[used++] = "--connect";' in launch_args
assert 'argv[used++] = "--mitm-session";' in launch_args
assert "endpoint->mitm_session[0]" in launch_args
assert "cb_online_get_netplay_endpoint(&session, &endpoint" in source
assert "cb_online_mark_game_started(&session" in source
assert "cb_disc_fingerprint(" in source
assert "cb_online_set_disc_fingerprint(" in source
assert "host_disc_waiting" in source
assert "view.disc_match" in source
assert "disc_mismatch" in online_source
assert "MATCH STARTED - PRESS START, THEN SELECT THE SAME CUE TO JOIN" in source
assert "START JOIN MATCH  SQUARE REFRESH  CIRCLE BACK" in source
assert '"HOST" : "CLIENT"' in source
assert "HOST: CROSS CONFIRMS DISC; LAUNCHES WHEN ALL MATCH." in source
assert "CLIENT: CROSS CONFIRMS DISC; THEN WAITS FOR HOST." in source
assert "cb_online_cancel_game_start(&session" in source
assert "NETPLAY_DISCOVERY_MAX_ATTEMPTS" in source
assert "CB-%.12s-HOST" in source
assert "Netplay Host List" not in source
assert "CB_GAME_NETPLAY_PORT" in launch_args
assert "host_player_id" in online_source
assert 'get_string_field(json, tokens, count, room, "netplayId"' in online_source
assert '"gameStarted"' in online_source
assert "view->game_started" in online_source
assert "if (view->game_started) view->is_ready = 1;" in online_source
assert "cb_online_get_netplay_endpoint" in online_source
assert "cb_online_clear_disc_fingerprint" in online_source
assert "clear_disc_confirmation(" in source
assert source.count("clear_disc_confirmation(&session, &view, error, sizeof(error))") >= 4
