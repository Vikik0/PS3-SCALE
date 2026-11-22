#!/usr/bin/env python3
"""Read-only static audit for player-owned Clockwork drill PIP routing."""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path


def function_body(source: str, name: str) -> tuple[str, str]:
    match = re.search(r"(?m)^\s*" + re.escape(name) + r"\s*\(([^)]*)\)\s*\{", source)
    if not match:
        return "", ""
    signature = match.group(1).strip()
    start = match.end()
    depth = 1
    i = start
    state = "code"
    while i < len(source):
        ch = source[i]
        nxt = source[i + 1] if i + 1 < len(source) else ""
        if state == "line":
            if ch == "\n":
                state = "code"
        elif state == "block":
            if ch == "*" and nxt == "/":
                state = "code"
                i += 1
        elif state == "string":
            if ch == "\\":
                i += 1
            elif ch == '"':
                state = "code"
        else:
            if ch == "/" and nxt == "/":
                state = "line"
                i += 1
            elif ch == "/" and nxt == "*":
                state = "block"
                i += 1
            elif ch == '"':
                state = "string"
            elif ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
                if depth == 0:
                    return signature, source[start:i]
        i += 1
    return signature, ""


def audit(interior_source: str, pip_source: str) -> dict:
    enable_sig, enable = function_body(pip_source, "pip_enable")
    timer_sig, timer = function_body(pip_source, "pip_display_timer")
    corners_sig, corners = function_body(pip_source, "pip_remove_l_corners")

    checks = {
        "pip_enable_accepts_player": bool(re.search(r"\bplayer\b", enable_sig)),
        "pip_enable_falls_back_to_primary": bool(re.search(r"player\s*=\s*level\.player", enable)),
        "pip_enable_creates_pip_for_selected_player": "player newpip()" in enable,
        "pip_enable_records_current_owner": "level.pintoscreenedge_player = player" in enable,
        "pip_enable_stops_previous_owner_pip": (
            enable.count('level.pintoscreenedge notify( "stop_interference" );') >= 2
            and "level.pintoscreenedge.enable = 0" in enable
        ),
        "pip_timer_accepts_player": bool(re.search(r"\bplayer\b", timer_sig)),
        "pip_timer_rebinds_pip_to_player": "pip_enable( player )" in timer,
        "pip_timer_passes_player_to_corner_setup": bool(
            re.search(r"pip_remove_l_corners\s*\([^;]*,\s*player\s*\)", timer, re.S)
        ),
        "corner_setup_accepts_player": bool(re.search(r"\bplayer\b", corners_sig)),
        "corner_setup_falls_back_to_primary": bool(re.search(r"player\s*=\s*level\.player", corners)),
        "corner_origin_uses_selected_player": "level.pintoscreenedge.origin = player.origin" in corners,
        "corner_origin_not_hardcoded_to_primary": "level.pintoscreenedge.origin = level.player.origin" not in corners,
        "drill_call_passes_selected_owner": bool(re.search(
            r"pip_display_timer\s*\([^;]*,\s*drill_player\s*\)", interior_source, re.S
        )),
    }
    return {
        "status": "PASS" if all(checks.values()) else "BLOCKED",
        "checks": checks,
        "limitations": [
            "newhudelem rendering and split-screen per-viewport visibility still require in-game validation",
            "the audit does not prove native Player 2 creation or independent camera/HUD contexts",
        ],
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("interior_source", type=Path)
    parser.add_argument("pip_source", type=Path)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)
    try:
        result = audit(
            args.interior_source.read_text(encoding="utf-8"),
            args.pip_source.read_text(encoding="utf-8"),
        )
    except OSError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    rendered = json.dumps(result, ensure_ascii=False, indent=2)
    print(rendered if args.json else f"{result['status']}: {sum(result['checks'].values())}/{len(result['checks'])} checks")
    return 0 if result["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
