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
assert "cb_online_get_room(" not in heartbeat.group(1)), (
    "Heartbeat must return the player list itself; do not issue a duplicate room GET."
)

print("Native lobby polling and expired-session recovery checks passed.")
