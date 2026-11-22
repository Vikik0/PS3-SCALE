#!/usr/bin/env python3
"""Read-only audit for player-owned Clockwork NVG light/lever handling."""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path


def strip_comments(source: str) -> str:
    out: list[str] = []
    i = 0
    state = "code"
    while i < len(source):
        ch = source[i]
        nxt = source[i + 1] if i + 1 < len(source) else ""
        if state == "line":
            if ch == "\n":
                out.append(ch)
                state = "code"
            else:
                out.append(" ")
        elif state == "block":
            if ch == "*" and nxt == "/":
                out.extend("  ")
                i += 1
                state = "code"
            else:
                out.append("\n" if ch == "\n" else " ")
        elif state == "string":
            out.append(ch)
            if ch == "\\" and nxt:
                out.append(nxt)
                i += 1
            elif ch == '"':
                state = "code"
        else:
            if ch == "/" and nxt == "/":
                out.extend("  ")
                i += 1
                state = "line"
            elif ch == "/" and nxt == "*":
                out.extend("  ")
                i += 1
                state = "block"
            else:
                out.append(ch)
                if ch == '"':
                    state = "string"
        i += 1
    return "".join(out)


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


def audit(source: str) -> dict:
    clean = strip_comments(source)
    lever_sig, lever = function_body(clean, "player_lever_input")
    watcher_sig, watcher = function_body(clean, "watch_secondary_nvg_player")
    fx_sig, fx = function_body(clean, "player_nvg_light_fx")
    keep_sig, keep = function_body(clean, "keep_player_below_surface")
    checks = {
        "lever_entry_keeps_legacy_no_argument_api": lever_sig == "",
        "primary_player_receives_light_fx": bool(re.search(
            r"thread\s+player_nvg_light_fx\s*\(\s*level\.player\s*\)", lever
        )),
        "secondary_alias_receives_light_fx": bool(re.search(
            r"thread\s+player_nvg_light_fx\s*\(\s*level\.playengineeffects\s*\)", lever
        )),
        "secondary_player_is_validated_and_distinct": (
            "isdefined( level.playengineeffects )" in lever
            and "isplayer( level.playengineeffects )" in lever
            and "isalive( level.playengineeffects )" in lever
            and "level.playengineeffects != level.player" in lever
        ),
        "late_secondary_player_watcher_is_started_once": (
            "level.ghc_nvg_secondary_light_watcher_active" in lever
            and "thread watch_secondary_nvg_player()" in lever
        ),
        "secondary_watcher_waits_until_lights_on": (
            watcher_sig == ""
            and 'flag( "lights_on" )' in watcher
            and "wait 0.1" in watcher
        ),
        "secondary_watcher_revalidates_current_alias": (
            "var_0 = level.playengineeffects" in watcher
            and "isplayer( var_0 )" in watcher
            and "isalive( var_0 )" in watcher
            and "var_0 != level.player" in watcher
        ),
        "secondary_watcher_skips_existing_fx_owner": (
            "var_0.ghc_nvg_light_active" in watcher
            and "thread player_nvg_light_fx( var_0 )" in watcher
        ),
        "secondary_watcher_clears_its_guard": (
            "level.ghc_nvg_secondary_light_watcher_active = undefined" in watcher
        ),
        "fx_helper_accepts_explicit_player": fx_sig == "player",
        "fx_helper_validates_player_liveness": (
            "!isdefined( player )" in fx and "!isalive( player )" in fx
        ),
        "fx_helper_prevents_duplicate_threads": (
            bool(re.search(
                r"if\s*\(\s*isdefined\(\s*player\.ghc_nvg_light_active\s*\)\s*&&\s*player\.ghc_nvg_light_active\s*\)\s*return\s*;",
                fx,
            ))
            and "player.ghc_nvg_light_active = 1" in fx
            and "player.ghc_nvg_light_active = undefined" in fx
        ),
        "fx_initial_state_is_player_specific": (
            "player.ghc_nvg_light_on = !isdefined( player.nightvision_effectsoff )" in fx
        ),
        "fx_tracks_selected_player_pose": (
            "player getplayerangles()" in fx
            and "player.origin + ( 0, 0, 64 )" in fx
            and "level.player.origin" not in fx
            and "level.player getplayerangles()" not in fx
        ),
        "fx_stops_on_shared_lights_on_or_player_exit": (
            'flag( "lights_on" )' in fx
            and "&& isalive( player )" in fx
            and "var_0 delete()" in fx
        ),
        "fx_cleans_player_owned_state": (
            "player.ghc_nvg_light_on = undefined" in fx
            and "player.ghc_nvg_light_active = undefined" in fx
        ),
        "state_helper_accepts_explicit_player": keep_sig == "player",
        "state_helper_uses_player_command": (
            'player notifyonplayercommand( "nvg_toggle", "+actionslot 1" )' in keep
        ),
        "state_helper_polls_player_nvgs_not_shared_flag": (
            "player.ghc_nvg_light_on = !isdefined( player.nightvision_effectsoff )" in keep
            and 'flag( "nvg_light_on" )' not in keep
            and "level.nvg_light_on" not in keep
        ),
        "state_helper_exits_when_lights_on_or_player_invalid": (
            'flag( "lights_on" )' in keep
            and "&& isalive( player )" in keep
        ),
    }
    return {
        "status": "PASS" if all(checks.values()) else "BLOCKED",
        "checks": checks,
        "limitations": [
            "static source audit does not prove that the engine emits nightvision_effectsoff transitions for both local players",
            "per-viewport flashlight rendering and independent NVG state require a live PS3/RPCS3 Campaign test",
            "the audit does not prove native Campaign Player 2 creation",
        ],
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)
    try:
        report = audit(args.source.read_text(encoding="utf-8"))
    except OSError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    rendered = json.dumps(report, ensure_ascii=False, indent=2)
    print(rendered if args.json else f"{report['status']}: {sum(report['checks'].values())}/{len(report['checks'])} checks")
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
