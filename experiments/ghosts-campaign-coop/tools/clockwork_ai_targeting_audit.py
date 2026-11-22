#!/usr/bin/env python3
"""Read-only audit of Clockwork's enemy-to-player and ally-to-enemy target paths."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path


def function_body(source: str, name: str) -> str:
    match = re.search(r"(?m)^\s*" + re.escape(name) + r"\s*\([^\n)]*\)\s*\{", source)
    if not match:
        return ""
    open_brace = source.find("{", match.start(), match.end())
    depth = 1
    i = open_brace + 1
    state = "code"
    while i < len(source):
        ch = source[i]
        nxt = source[i + 1] if i + 1 < len(source) else ""
        if state == "line":
            if ch == "\n": state = "code"
        elif state == "block":
            if ch == "*" and nxt == "/": state = "code"; i += 1
        elif state == "string":
            if ch == "\\": i += 1
            elif ch == '"': state = "code"
        else:
            if ch == "/" and nxt == "/": state = "line"; i += 1
            elif ch == "/" and nxt == "*": state = "block"; i += 1
            elif ch == '"': state = "string"
            elif ch == "{": depth += 1
            elif ch == "}":
                depth -= 1
                if depth == 0: return source[open_brace + 1:i]
        i += 1
    return ""


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def audit(interior: str, nvg: str, code: str) -> dict:
    discovery = function_body(interior, "discovery_guys_wakeup")
    allies = function_body(code, "attack_targets")
    dialogue = function_body(nvg, "begin_interior")
    audio = function_body(code, "listen_player_press_x")
    checks = {
        "discovery_ai_has_one_primary_only_enemy_target_call": (
            discovery.count("getenemyinfo( level.player )") == 1
            and "getenemyinfo( level.playengineeffects )" not in discovery
        ),
        "discovery_target_assignment_is_inside_filtered_ai_loop": (
            "foreach ( var_1 in level.discovery_guys )" in discovery
            and "var_1 getenemyinfo( level.player )" in discovery
        ),
        "shared_ally_attack_targets_ai_group_not_player": (
            "var_0[var_7] getenemyinfo( var_1[var_6] )" in allies
            and "level.player" not in allies
            and "level.playengineeffects" not in allies
        ),
        "intro_closest_ai_call_is_dialogue_only": (
            'get_closest_ai( level.player.origin, "axis" )' in dialogue
            and "char_dialog_add_and_go" in dialogue
            and "getenemyinfo" not in dialogue
        ),
        "snowmobile_player_distance_is_audio_only": (
            "distance( level.player.origin, self.origin )" in audio
            and "chase_crashmix" in audio
            and "getenemyinfo" not in audio
        ),
    }
    return {
        "status": "PASS" if all(checks.values()) else "BLOCKED",
        "checks": checks,
        "classification": {
            "enemy_to_player_targeting": "discovery_guys_wakeup is the only direct Clockwork mission assignment found; it currently targets Logan once per awakened AI",
            "ally_to_enemy_targeting": "attack_targets is shared mission combat orchestration and should remain a single ally-to-AI group assignment",
            "story_dialogue": "begin_interior selects the closest axis AI to Logan for a scripted line; keep primary-player attribution unless cinematic ownership is redesigned",
            "audio_distance": "listen_player_press_x uses Logan's distance to choose a snowmobile crash audio mix; this is not AI target acquisition",
        },
        "recommended_patch": {
            "function": "discovery_guys_wakeup",
            "policy": "choose the nearest currently valid local player independently for each AI at wake-up, then call getenemyinfo exactly once for that AI",
            "fallback": "use Logan if valid; use the secondary player if Logan is invalid; skip assignment only if neither native player is valid",
            "not_in_scope": ["continuous per-frame retargeting", "duplicating attack_targets", "changing scripted dialogue/audio ownership", "creating Player 2"],
        },
        "limitations": [
            "this is a static source audit; actual AI aggro, perception and target switching require live Campaign testing",
            "nearest-player selection occurs once at wake-up and does not dynamically retarget after players move",
            "the native secondary local-player creation path remains a prerequisite",
        ],
    }


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("interior", type=Path)
    parser.add_argument("interior_nvg", type=Path)
    parser.add_argument("clockwork_code", type=Path)
    parser.add_argument("--output-json", type=Path)
    args = parser.parse_args(argv)
    try:
        sources = [p.read_text(encoding="utf-8") for p in (args.interior, args.interior_nvg, args.clockwork_code)]
        report = audit(*sources)
        report["inputs"] = {
            p.name: {"bytes": p.stat().st_size, "sha256": sha(p)}
            for p in (args.interior, args.interior_nvg, args.clockwork_code)
        }
        rendered = json.dumps(report, ensure_ascii=False, indent=2) + "\n"
        if args.output_json:
            args.output_json.parent.mkdir(parents=True, exist_ok=True)
            args.output_json.write_text(rendered, encoding="utf-8")
        print(rendered, end="")
        return 0 if report["status"] == "PASS" else 1
    except OSError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
