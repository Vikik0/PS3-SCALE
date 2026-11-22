#!/usr/bin/env python3
# Reconstruct the documented Clockwork Stage 5/6 source changes on the pinned Stage 2 GSC.
from __future__ import annotations
import argparse, hashlib, json, re, sys
from pathlib import Path

EXPECTED_STAGE2_SHA256 = "d2411ee69e328c6932f53ecc5db8e547bad465379a8cd9593fa1f171f0a5c73b"

HELPERS = r'''clockwork_get_secondary_player()
{
    if ( !issplitscreen() || !isdefined( level.playengineeffects ) )
        return undefined;

    secondary_player = level.playengineeffects;

    if ( !isdefined( secondary_player ) || !isplayer( secondary_player ) || !isalive( secondary_player ) || secondary_player == level.player || !secondary_player issplitscreenplayer() || secondary_player issplitscreenplayerprimary() )
        return undefined;

    return secondary_player;
}

clockwork_apply_vault_blast( player, blast_origin )
{
    if ( !isdefined( player ) || !isplayer( player ) || !isalive( player ) || !isdefined( blast_origin ) )
        return;

    blast_damage = player.health - 5;

    if ( blast_damage < 0 )
        blast_damage = 0;

    player dodamage( blast_damage, blast_origin );
    player shellshock( "default", 5 );
    player setstance( "prone" );
    player pushplayervector( ( 100, 0, 0 ), 1 );
    wait 0.2;

    if ( isdefined( player ) && isalive( player ) )
        player pushplayervector( ( 0, 0, 0 ), 1 );
}'''

NEW_BLAST_BLOCK = r'''if ( common_scripts\utility::flag( "vault_blast_area" ) )
    {
        var_3 = getent( "smoke_spot", "targetname" );
        secondary_player = clockwork_get_secondary_player();

        if ( isdefined( secondary_player ) )
            thread clockwork_apply_vault_blast( secondary_player, var_3.origin );

        clockwork_apply_vault_blast( level.player, var_3.origin );
    }

    level.player playrumbleonentity( "artillery_rumble" );
    secondary_player = clockwork_get_secondary_player();

    if ( isdefined( secondary_player ) )
        secondary_player playrumbleonentity( "artillery_rumble" );'''

def function_span(source: str, name: str):
    match = re.search(r"(?m)^" + re.escape(name) + r"\s*\([^\n)]*\)\s*\{", source)
    if not match:
        raise ValueError(f"missing function: {name}")
    opening = source.find("{", match.start(), match.end())
    depth, i, state = 1, opening + 1, "code"
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
                if depth == 0: return match.start(), i + 1, opening, source[opening + 1:i]
        i += 1
    raise ValueError(f"unterminated function: {name}")

def replace_in_function(source: str, name: str, old: str, new: str, expected_count: int = 1) -> str:
    _, end, opening, body = function_span(source, name)
    count = body.count(old)
    if count != expected_count:
        raise ValueError(f"{name}: expected {expected_count} pinned block occurrence(s), found {count}")
    body = body.replace(old, new, expected_count)
    return source[:opening + 1] + body + source[end - 1:end] + source[end:]

def replace_function(source: str, name: str, replacement: str) -> str:
    start, end, _, _ = function_span(source, name)
    return source[:start] + replacement + source[end:]

def apply_stage5(source: str) -> str:
    if "clockwork_get_secondary_player(" in source or "clockwork_apply_vault_blast(" in source:
        raise ValueError("Stage 5 helpers already exist; refusing duplicate patch")
    old = r'''if ( common_scripts\utility::flag( "vault_blast_area" ) )
    {
        var_3 = getent( "smoke_spot", "targetname" );
        level.player dodamage( level.player.health - 5, var_3.origin );
        level.player shellshock( "default", 5 );
        level.player setstance( "prone" );
        level.player pushplayervector( ( 100, 0, 0 ), 1 );
        wait 0.2;
        level.player pushplayervector( ( 0, 0, 0 ), 1 );
    }

    level.player playrumbleonentity( "artillery_rumble" );'''
    source = replace_in_function(source, "new_trench_setup", old, NEW_BLAST_BLOCK)
    insert_at, _, _, _ = function_span(source, "new_trench_setup")
    return source[:insert_at] + HELPERS + "\n\n" + source[insert_at:]

def apply_stage6(source: str) -> str:
    source = replace_in_function(source, "setup_interior_cqb",
        '    level.player switchtoweapon( "cz805bren+reflex_sp+silencer_sp" );',
        '''    level.player switchtoweapon( "cz805bren+reflex_sp+silencer_sp" );
    secondary_player = clockwork_get_secondary_player();

    if ( isdefined( secondary_player ) )
    {
        secondary_player switchtoweapon( "cz805bren+reflex_sp+silencer_sp" );
        secondary_player setactionslot( 1, "nightvision" );
        secondary_player thread maps\\_nightvision::nightvision_off();
        secondary_player nightvisiongogglesforceon();
    }''')
    source = replace_in_function(source, "setup_interior",
        '    level.player switchtoweapon( "cz805bren+reflex_sp+silencer_sp" );',
        '''    level.player switchtoweapon( "cz805bren+reflex_sp+silencer_sp" );
    secondary_player = clockwork_get_secondary_player();

    if ( isdefined( secondary_player ) )
        secondary_player switchtoweapon( "cz805bren+reflex_sp+silencer_sp" );''')
    source = replace_function(source, "vault_nag", r'''vault_nag()
{
    common_scripts\utility::flag_wait( "at_vault_door" );
    level.player thread maps\_utility::display_hint_timeout( "disable_nvg", 5 );
    secondary_player = clockwork_get_secondary_player();

    if ( isdefined( secondary_player ) )
        secondary_player thread maps\_utility::display_hint_timeout( "disable_nvg", 5 );

    common_scripts\utility::flag_wait( "approaching_vault_door" );

    secondary_player = clockwork_get_secondary_player();

    while ( level.player isswitchingweapon() || ( isdefined( secondary_player ) && secondary_player isswitchingweapon() ) )
    {
        wait 0.01;
        secondary_player = clockwork_get_secondary_player();
    }

    thread maps\clockwork_code::nvg_blackout_anims();
}''')
    source = replace_in_function(source, "animate_vault_door",
        '    common_scripts\\utility::flag_wait( "glow_start" );\n    level.player thread maps\\_utility::display_hint( "disable_nvg" );',
        '''    common_scripts\\utility::flag_wait( "glow_start" );
    level.player thread maps\\_utility::display_hint( "disable_nvg" );
    secondary_player = clockwork_get_secondary_player();

    if ( isdefined( secondary_player ) )
        secondary_player thread maps\\_utility::display_hint( "disable_nvg" );''')
    source = replace_in_function(source, "animate_vault_door",
        '    level.player setactionslot( 1, "" );',
        '''    level.player setactionslot( 1, "" );
    secondary_player = clockwork_get_secondary_player();

    if ( isdefined( secondary_player ) )
        secondary_player setactionslot( 1, "" );''', expected_count=2)
    source = replace_in_function(source, "vault_props",
        '''    level.grenades = level.player getweaponammoclip( "fraggrenade" );
    level.flashbangs = level.player getweaponammoclip( "flash_grenade" );
    level.player takeweapon( "fraggrenade" );
    level.player takeweapon( "flash_grenade" );''',
        '''    level.grenades = level.player getweaponammoclip( "fraggrenade" );
    level.flashbangs = level.player getweaponammoclip( "flash_grenade" );
    level.player takeweapon( "fraggrenade" );
    level.player takeweapon( "flash_grenade" );

    level.ghc_vault_player_two = undefined;
    level.ghc_vault_player_two_grenades = 0;
    level.ghc_vault_player_two_flashbangs = 0;
    secondary_player = clockwork_get_secondary_player();

    if ( isdefined( secondary_player ) )
    {
        level.ghc_vault_player_two = secondary_player;
        level.ghc_vault_player_two_grenades = secondary_player getweaponammoclip( "fraggrenade" );
        level.ghc_vault_player_two_flashbangs = secondary_player getweaponammoclip( "flash_grenade" );
        secondary_player takeweapon( "fraggrenade" );
        secondary_player takeweapon( "flash_grenade" );
    }''')
    source = replace_in_function(source, "animate_vault_door",
        '    level.player setweaponammoclip( "flash_grenade", level.flashbangs );',
        '''    level.player setweaponammoclip( "flash_grenade", level.flashbangs );
    secondary_player = clockwork_get_secondary_player();

    if ( isdefined( level.ghc_vault_player_two ) && isdefined( secondary_player ) && secondary_player == level.ghc_vault_player_two && isalive( secondary_player ) )
    {
        secondary_player giveweapon( "fraggrenade" );
        secondary_player setweaponammoclip( "fraggrenade", level.ghc_vault_player_two_grenades );
        secondary_player setweaponammostock( "fraggrenade", level.ghc_vault_player_two_grenades );
        secondary_player giveweapon( "flash_grenade" );
        secondary_player setweaponammoclip( "flash_grenade", level.ghc_vault_player_two_flashbangs );
        secondary_player setweaponammostock( "flash_grenade", level.ghc_vault_player_two_flashbangs );
    }

    level.ghc_vault_player_two = undefined;''')
    source = replace_in_function(source, "handle_cqb_allies",
        '    level.player allowsprint( 1 );\n    level.player allowjump( 1 );',
        '''    level.player allowsprint( 1 );
    level.player allowjump( 1 );
    secondary_player = clockwork_get_secondary_player();

    if ( isdefined( secondary_player ) )
    {
        secondary_player allowsprint( 1 );
        secondary_player allowjump( 1 );
    }''')
    source = replace_in_function(source, "cqb_dog",
        '        level.player notify( "dog_attack_override" );',
        '''        level.player notify( "dog_attack_override" );
        secondary_player = clockwork_get_secondary_player();

        if ( isdefined( secondary_player ) )
            secondary_player notify( "dog_attack_override" );''')
    return source

def patch_source(source: str, expected_sha256: str = EXPECTED_STAGE2_SHA256):
    actual = hashlib.sha256(source.encode("utf-8")).hexdigest()
    if actual != expected_sha256:
        raise ValueError(f"Stage 2 source SHA-256 mismatch: expected {expected_sha256}, got {actual}")
    stage5 = apply_stage5(source)
    stage6 = apply_stage6(stage5)
    required = [
        "clockwork_apply_vault_blast( level.player, var_3.origin )",
        "secondary_player allowsprint( 1 )",
        "secondary_player allowjump( 1 )",
        "level.ghc_vault_player_two = secondary_player",
        'secondary_player notify( "dog_attack_override" )',
    ]
    for marker in required:
        if marker not in stage6: raise ValueError(f"post-patch validation missing marker: {marker}")
    return stage5, stage6

def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="pinned Stage 2 Clockwork interior GSC")
    parser.add_argument("output", type=Path, help="new reconstructed Stage 6 source path")
    parser.add_argument("--stage5-output", type=Path)
    parser.add_argument("--report-json", type=Path)
    args = parser.parse_args(argv)
    try:
        raw = args.source.read_bytes()
        if args.source.resolve() == args.output.resolve() or (args.stage5_output and args.source.resolve() == args.stage5_output.resolve()):
            raise ValueError("output must not overwrite the input source")
        if args.output.exists() or (args.stage5_output and args.stage5_output.exists()):
            raise ValueError("output path already exists; choose a new path")
        stage5, stage6 = patch_source(raw.decode("utf-8"))
        b5, b6 = stage5.encode("utf-8"), stage6.encode("utf-8")
        if args.stage5_output:
            args.stage5_output.parent.mkdir(parents=True, exist_ok=True)
            args.stage5_output.write_bytes(b5)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(b6)
        report = {
            "status":"RECONSTRUCTED",
            "input_sha256":hashlib.sha256(raw).hexdigest(),
            "stage5_output_bytes":len(b5),
            "stage5_output_sha256":hashlib.sha256(b5).hexdigest(),
            "stage6_output_bytes":len(b6),
            "stage6_output_sha256":hashlib.sha256(b6).hexdigest(),
            "source_lineage":"Stage 2 -> reconstructed Stage 5 -> reconstructed Stage 6",
            "note":"Behavior-based reconstruction from Phase 18/19 documentation; not the recovered historical Stage 6 file."
        }
        rendered=json.dumps(report,ensure_ascii=False,indent=2)+"\n"
        if args.report_json:
            args.report_json.parent.mkdir(parents=True,exist_ok=True)
            args.report_json.write_text(rendered,encoding="utf-8")
        print(rendered,end="")
        return 0
    except (OSError,UnicodeDecodeError,ValueError) as exc:
        print(f"error: {exc}",file=sys.stderr)
        return 1

if __name__ == "__main__":
    raise SystemExit(main())
