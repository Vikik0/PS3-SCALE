#!/usr/bin/env python3
"""Read-only audit for reconstructed Clockwork Stage 5/6 player-scoped changes."""
from __future__ import annotations
import argparse, json, re, sys
from pathlib import Path

def function_body(source: str, name: str) -> str:
    m=re.search(r"(?m)^"+re.escape(name)+r"\s*\([^\n)]*\)\s*\{",source)
    if not m:return ""
    opening=source.find("{",m.start(),m.end()); depth=1; i=opening+1; state="code"
    while i<len(source):
        ch=source[i]; nxt=source[i+1] if i+1<len(source) else ""
        if state=="line":
            if ch=="\n":state="code"
        elif state=="block":
            if ch=="*" and nxt=="/":state="code"; i+=1
        elif state=="string":
            if ch=="\\":i+=1
            elif ch=='"':state="code"
        else:
            if ch=="/" and nxt=="/":state="line"; i+=1
            elif ch=="/" and nxt=="*":state="block"; i+=1
            elif ch=='"':state="string"
            elif ch=="{":depth+=1
            elif ch=="}":
                depth-=1
                if depth==0:return source[opening+1:i]
        i+=1
    return ""

def audit(source: str) -> dict:
    s= function_body(source,"clockwork_get_secondary_player")
    b= function_body(source,"clockwork_apply_vault_blast")
    t= function_body(source,"new_trench_setup")
    c= function_body(source,"setup_interior_cqb")
    i= function_body(source,"setup_interior")
    n= function_body(source,"vault_nag")
    d= function_body(source,"animate_vault_door")
    p= function_body(source,"vault_props")
    h= function_body(source,"handle_cqb_allies")
    dog=function_body(source,"cqb_dog")
    checks={
      "secondary_alias_requires_native_split_screen_identity":"issplitscreen()" in s and "secondary_player issplitscreenplayer()" in s and "secondary_player issplitscreenplayerprimary()" in s and "secondary_player == level.player" in s,
      "vault_blast_clamps_negative_damage":"blast_damage = player.health - 5" in b and "if ( blast_damage < 0 )" in b and "blast_damage = 0" in b,
      "vault_blast_clears_push_only_while_owner_alive":"wait 0.2" in b and "isdefined( player ) && isalive( player )" in b and "player pushplayervector( ( 0, 0, 0 ), 1 )" in b,
      "vault_blast_is_routed_to_both_players":"clockwork_apply_vault_blast( level.player, var_3.origin )" in t and "clockwork_apply_vault_blast( secondary_player, var_3.origin )" in t,
      "artillery_rumble_is_routed_to_both_players":'level.player playrumbleonentity( "artillery_rumble" )' in t and 'secondary_player playrumbleonentity( "artillery_rumble" )' in t,
      "secondary_interior_cqb_weapon_and_nvg_are_configured":'secondary_player switchtoweapon( "cz805bren+reflex_sp+silencer_sp" )' in c and 'secondary_player setactionslot( 1, "nightvision" )' in c and "secondary_player thread maps\\_nightvision::nightvision_off()" in c and "secondary_player nightvisiongogglesforceon()" in c,
      "secondary_post_vault_weapon_is_configured":'secondary_player switchtoweapon( "cz805bren+reflex_sp+silencer_sp" )' in i,
      "vault_nag_targets_both_and_waits_for_both_switches":'secondary_player thread maps\\_utility::display_hint_timeout( "disable_nvg", 5 )' in n and "secondary_player isswitchingweapon()" in n,
      "vault_door_sends_secondary_hint_and_clears_action_slot":'secondary_player thread maps\\_utility::display_hint( "disable_nvg" )' in d and 'secondary_player setactionslot( 1, "" )' in d,
      "vault_props_saves_secondary_identity_and_ammo":"level.ghc_vault_player_two = secondary_player" in p and 'level.ghc_vault_player_two_grenades = secondary_player getweaponammoclip( "fraggrenade" )' in p and 'level.ghc_vault_player_two_flashbangs = secondary_player getweaponammoclip( "flash_grenade" )' in p and 'secondary_player takeweapon( "fraggrenade" )' in p and 'secondary_player takeweapon( "flash_grenade" )' in p,
      "vault_door_restores_only_the_same_secondary_entity":"secondary_player == level.ghc_vault_player_two" in d and "level.ghc_vault_player_two = undefined" in d and 'secondary_player setweaponammostock( "fraggrenade", level.ghc_vault_player_two_grenades )' in d,
      "cqb_allies_enable_secondary_sprint_and_jump":"secondary_player allowsprint( 1 )" in h and "secondary_player allowjump( 1 )" in h,
      "dog_attack_override_notifies_secondary":'secondary_player notify( "dog_attack_override" )' in dog
    }
    return {"status":"PASS" if all(checks.values()) else "BLOCKED","checks":checks}

def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source",type=Path)
    parser.add_argument("--output-json",type=Path)
    args=parser.parse_args(argv)
    try:
        report=audit(args.source.read_text(encoding="utf-8"))
        report["source_bytes"]=args.source.stat().st_size
        rendered=json.dumps(report,ensure_ascii=False,indent=2)+"\n"
        if args.output_json:
            args.output_json.parent.mkdir(parents=True,exist_ok=True)
            args.output_json.write_text(rendered,encoding="utf-8")
        print(rendered,end="")
        return 0 if report["status"]=="PASS" else 1
    except OSError as exc:
        print(f"error: {exc}",file=sys.stderr); return 2

if __name__=="__main__":
    raise SystemExit(main())
