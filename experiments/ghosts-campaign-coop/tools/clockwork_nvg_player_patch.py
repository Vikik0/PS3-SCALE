#!/usr/bin/env python3
"""Apply the Stage 7 player-owned NVG light patch to the pinned IW6 source."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

EXPECTED_SOURCE_SHA256 = "6d4617f65e5fd0ba91f85ebf6e2b35a57901cc1ebdce85c8899c8781815cf096"

PLAYER_LEVER_INPUT = r'''player_lever_input()
{
    if ( level.x_turretcheck || level.ps3 )
    {
        if ( isdefined( level.player ) && isalive( level.player ) )
            thread player_nvg_light_fx( level.player );

        if ( isdefined( level.playengineeffects ) && isplayer( level.playengineeffects ) && isalive( level.playengineeffects ) && level.playengineeffects != level.player )
            thread player_nvg_light_fx( level.playengineeffects );

        if ( !isdefined( level.ghc_nvg_secondary_light_watcher_active ) || !level.ghc_nvg_secondary_light_watcher_active )
        {
            level.ghc_nvg_secondary_light_watcher_active = 1;
            thread watch_secondary_nvg_player();
        }
    }
}'''

WATCH_SECONDARY_NVG_PLAYER = r'''watch_secondary_nvg_player()
{
    while ( !common_scripts\utility::flag( "lights_on" ) )
    {
        if ( isdefined( level.playengineeffects ) )
        {
            var_0 = level.playengineeffects;

            if ( isdefined( var_0 ) && isplayer( var_0 ) && isalive( var_0 ) && var_0 != level.player )
            {
                if ( !isdefined( var_0.ghc_nvg_light_active ) || !var_0.ghc_nvg_light_active )
                    thread player_nvg_light_fx( var_0 );
            }
        }

        wait 0.1;
    }

    level.ghc_nvg_secondary_light_watcher_active = undefined;
}'''

PLAYER_NVG_LIGHT_FX = r'''player_nvg_light_fx( player )
{
    if ( !isdefined( player ) || !isalive( player ) )
        return;

    if ( isdefined( player.ghc_nvg_light_active ) && player.ghc_nvg_light_active )
        return;

    player.ghc_nvg_light_active = 1;
    player.ghc_nvg_light_on = !isdefined( player.nightvision_effectsoff );
    var_0 = common_scripts\utility::spawn_tag_fx();
    playfxontag( level._effect["player_nvg_light"], var_0, "tag_origin" );
    thread keep_player_below_surface( player );

    while ( !common_scripts\utility::flag( "lights_on" ) && isdefined( player ) && isalive( player ) )
    {
        if ( isdefined( player.ghc_nvg_light_on ) && player.ghc_nvg_light_on )
        {
            var_0.angles = player getplayerangles();
            var_0.origin = player.origin + ( 0, 0, 64 );
        }
        else
            var_0.origin = ( 0, 0, 0 );

        wait 0.01;
    }

    if ( isdefined( var_0 ) )
        var_0 delete();

    if ( isdefined( player ) )
    {
        player.ghc_nvg_light_active = undefined;
        player.ghc_nvg_light_on = undefined;
    }
}'''

KEEP_PLAYER_BELOW_SURFACE = r'''keep_player_below_surface( player )
{
    if ( !isdefined( player ) || !isalive( player ) )
        return;

    player notifyonplayercommand( "nvg_toggle", "+actionslot 1" );

    while ( !common_scripts\utility::flag( "lights_on" ) && isdefined( player ) && isalive( player ) )
    {
        player.ghc_nvg_light_on = !isdefined( player.nightvision_effectsoff );
        wait 0.05;
    }
}'''


def replace_function(source: str, name: str, replacement: str) -> str:
    match = re.search(
        r"(?m)^" + re.escape(name) + r"\s*\([^\n)]*\)\s*\{",
        source,
    )
    if not match:
        raise ValueError(f"missing function: {name}")
    line_start = match.start()
    brace = source.find("{", match.start(), match.end())
    if brace < 0:
        raise ValueError(f"missing function body: {name}")
    depth = 1
    i = brace + 1
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
                    return source[:line_start] + replacement + source[i + 1:]
        i += 1
    raise ValueError(f"unterminated function: {name}")


def patch_source(source: str) -> str:
    patched = replace_function(source, "player_lever_input", PLAYER_LEVER_INPUT)
    patched = replace_function(patched, "keep_player_below_surface", KEEP_PLAYER_BELOW_SURFACE)
    anchor = "nvg_animted_scene()\n{"
    if anchor not in patched:
        raise ValueError("missing nvg_animted_scene insertion point")
    helpers = WATCH_SECONDARY_NVG_PLAYER + "\n\n" + PLAYER_NVG_LIGHT_FX
    return patched.replace(anchor, helpers + "\n\n" + anchor, 1)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="original maps__clockwork_interior_nvg.gsc")
    parser.add_argument("output", type=Path, help="new patched source path; input is never overwritten")
    parser.add_argument("--report-json", type=Path)
    args = parser.parse_args(argv)
    try:
        raw = args.source.read_bytes()
        source_hash = hashlib.sha256(raw).hexdigest()
        if source_hash != EXPECTED_SOURCE_SHA256:
            raise ValueError(
                "source SHA-256 does not match the pinned Clockwork analysis input; "
                "refusing to patch an unknown game build"
            )
        if args.source.resolve() == args.output.resolve():
            raise ValueError("output must not overwrite the input source")
        if args.output.exists():
            raise ValueError("output already exists; choose a new path")
        patched = patch_source(raw.decode("utf-8"))
        output_bytes = patched.encode("utf-8")
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(output_bytes)
        report = {
            "status": "PATCHED",
            "input": str(args.source),
            "input_sha256": source_hash,
            "output": str(args.output),
            "output_sha256": hashlib.sha256(output_bytes).hexdigest(),
            "output_bytes": len(output_bytes),
            "changes": [
                "route the player-owned NVG light FX to Logan and the native secondary player",
                "watch for a late native secondary-player binding without starting duplicate threads",
                "track NVG on/off state on each player entity rather than a shared level flag",
                "stop and clean up the FX when lights turn on or its owner dies/disconnects",
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
