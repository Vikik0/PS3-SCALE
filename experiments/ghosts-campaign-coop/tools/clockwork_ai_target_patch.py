#!/usr/bin/env python3
"""Patch only Clockwork's discovery enemy target selection, preserving other edits."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

EXPECTED_DISCOVERY_BODY_SHA256 = "9003356c9a048664f9d09274d0146043df0fc011caae2a83555b4335ab76f56a"

TARGET_CALL_OLD = "var_1 getenemyinfo( level.player );"
TARGET_CALL_NEW = """var_2 = clockwork_pick_enemy_target( var_1 );
        if ( isdefined( var_2 ) && isplayer( var_2 ) && isalive( var_2 ) )
            var_1 getenemyinfo( var_2 );"""

PICK_TARGET_HELPER = r'''clockwork_pick_enemy_target( ai )
{
    if ( !isdefined( ai ) || !isalive( ai ) )
        return undefined;

    target_player = undefined;

    if ( isdefined( level.player ) && isplayer( level.player ) && isalive( level.player ) )
        target_player = level.player;

    if ( issplitscreen() && isdefined( level.playengineeffects ) )
    {
        secondary_player = level.playengineeffects;

        if ( isdefined( secondary_player ) && isplayer( secondary_player ) && isalive( secondary_player ) && secondary_player != target_player && secondary_player issplitscreenplayer() && !secondary_player issplitscreenplayerprimary() )
        {
            if ( !isdefined( target_player ) )
                target_player = secondary_player;
            else if ( distance( ai.origin, secondary_player.origin ) < distance( ai.origin, target_player.origin ) )
                target_player = secondary_player;
        }
    }

    return target_player;
}'''


def function_span(source: str, name: str) -> tuple[int, int, int, str]:
    match = re.search(r"(?m)^" + re.escape(name) + r"\s*\([^\n)]*\)\s*\{", source)
    if not match:
        raise ValueError(f"missing function: {name}")
    open_brace = source.find("{", match.start(), match.end())
    depth = 1
    i = open_brace + 1
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
                    return match.start(), i + 1, open_brace, source[open_brace + 1:i]
        i += 1
    raise ValueError(f"unterminated function: {name}")


def patch_source(source: str, expected_body_sha256: str = EXPECTED_DISCOVERY_BODY_SHA256) -> str:
    if re.search(r"(?m)^clockwork_pick_enemy_target\s*\(", source):
        raise ValueError("clockwork_pick_enemy_target already exists; refusing a duplicate patch")
    start, end, open_brace, body = function_span(source, "discovery_guys_wakeup")
    body_hash = hashlib.sha256(body.encode("utf-8")).hexdigest()
    if body_hash != expected_body_sha256:
        raise ValueError(
            "discovery_guys_wakeup body does not match the reviewed baseline; "
            "refusing to patch a changed mission sequence"
        )
    if body.count(TARGET_CALL_OLD) != 1:
        raise ValueError("expected exactly one primary-only discovery getenemyinfo call")
    patched_body = body.replace(TARGET_CALL_OLD, TARGET_CALL_NEW, 1)
    patched_function = source[start:open_brace + 1] + patched_body + source[end - 1:end]
    return source[:start] + PICK_TARGET_HELPER + "\n\n" + patched_function + source[end:]


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="Stage 2-patched Clockwork maps__clockwork_interior.gsc source")
    parser.add_argument("output", type=Path, help="new source path; input is never overwritten")
    parser.add_argument("--report-json", type=Path)
    args = parser.parse_args(argv)
    try:
        raw = args.source.read_bytes()
        if args.source.resolve() == args.output.resolve():
            raise ValueError("output must not overwrite the input source")
        if args.output.exists():
            raise ValueError("output already exists; choose a new path")
        source = raw.decode("utf-8")
        _, _, _, baseline_body = function_span(source, "discovery_guys_wakeup")
        baseline_hash = hashlib.sha256(baseline_body.encode("utf-8")).hexdigest()
        patched = patch_source(source)
        output = patched.encode("utf-8")
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(output)
        report = {
            "status": "PATCHED",
            "input": str(args.source),
            "input_sha256": hashlib.sha256(raw).hexdigest(),
            "discovery_function_body_sha256": baseline_hash,
            "output": str(args.output),
            "output_sha256": hashlib.sha256(output).hexdigest(),
            "output_bytes": len(output),
            "behavior": "each discovery AI receives one enemy target: the nearest currently valid native local player at wake-up",
            "preserved": [
                "the shared discovery_guys flag and wake-up sequence",
                "ally-to-AI attack_targets behavior",
                "primary-player story dialogue and audio-distance cues",
                "all unrelated source functions, including prior drill/blast/interior patches",
            ],
        }
        rendered = json.dumps(report, ensure_ascii=False, indent=2) + "\n"
        if args.report_json:
            args.report_json.parent.mkdir(parents=True, exist_ok=True)
            args.report_json.write_text(rendered, encoding="utf-8")
        print(rendered, end="")
        return 0
    except (OSError, UnicodeDecodeError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
