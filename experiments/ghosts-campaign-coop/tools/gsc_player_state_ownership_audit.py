#!/usr/bin/env python3
"""Classify per-player API use versus level-scope state in selected IW6 GSC functions.

This is a read-only static audit. It does not rewrite scripts or assume that
level.player references can be mechanically replaced with a second player.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from collections import Counter
from pathlib import Path
from typing import Any

try:
    from .gsc_callgraph_audit import clean_source_lines, function_ranges
except ImportError:
    from gsc_callgraph_audit import clean_source_lines, function_ranges

PLAYER_RE = re.compile(r"\blevel\.player\b")
PLAYER_METHOD_RE = re.compile(
    r"\blevel\.player\s+(?:(?:[A-Za-z_][A-Za-z0-9_]*\\)*[A-Za-z_][A-Za-z0-9_]*::)?([A-Za-z_][A-Za-z0-9_]*)\s*\("
)
PLAYER_FIELD_RE = re.compile(r"\blevel\.player\.([A-Za-z_][A-Za-z0-9_]*)")
PLAYER_ARRAY_PATTERNS = {
    "level.playerpiggyback": re.compile(r"\blevel\.playerpiggyback\b"),
    "level.players": re.compile(r"\blevel\.players\b"),
}
SECONDARY_CLIENT_RE = re.compile(r"\blevel\.playengineeffects\b")
LEVEL_FIELD_RE = re.compile(r"\blevel\.([A-Za-z_][A-Za-z0-9_]*)")
FLAG_API_RE = re.compile(r"\b(?:flag_init|flag_set|flag_clear|flag_wait|flag_wait_all|flag_wait_any|flag_waitopen)\s*\(", re.I)
DVAR_API_RE = re.compile(r"\b(?:setdvar|setsaveddvar|getdvar|getdvarint)\s*\(", re.I)

MISSION_FAILURE_RE = re.compile(r"\bmissionfailed\s*\(", re.I)
AUTOSAVE_RE = re.compile(r"\bautosave(?:_[A-Za-z0-9_]+)?\s*\(", re.I)
SLOWMO_RE = re.compile(r"\bslowmo_[A-Za-z0-9_]+\s*\(", re.I)
WORLD_PHYSICS_RE = re.compile(r"\b(?:setphysicsgravitydir|physicsexplosionsphere|earthquake)\s*\(", re.I)
LEVEL_EVENT_RE = re.compile(r"\blevel\s+(?:notify|waittill|endon)\s*\(", re.I)
LEVEL_FIELD_WRITE_RE = re.compile(r"\blevel\.([A-Za-z_][A-Za-z0-9_]*)\s*(?:\+\+|--|[+*/-]?=)")
PLAYER_FIELD_WRITE_RE = re.compile(r"\blevel\.player\.([A-Za-z_][A-Za-z0-9_]*)\s*(?:\+\+|--|[+*/-]?=)")
ANIM_RIG_RE = re.compile(r"\bspawn_anim_ent\s*\(", re.I)

METHOD_GROUPS = {
    "input": {
        "buttonpressed", "attackbuttonpressed", "adsbuttonpressed", "usebuttonpressed",
        "meleebuttonpressed", "jumpbuttonpressed", "reloadbuttonpressed", "issprinting",
    },
    "weapons": {
        "giveweapon", "takeweapon", "takeallweapons", "switchtoweapon", "switchtoweaponimmediate",
        "getcurrentweapon", "setweaponammoclip", "setweaponammostock", "getweaponammoclip",
        "getweaponammostock", "getweaponslistprimaries", "disableweapons", "enableweapons",
        "disableweaponswitch", "enableweaponswitch", "disableoffhandweapons", "enableoffhandweapons",
        "allowfire", "disableweaponpickup", "enableweaponpickup", "setperk", "unsetperk",
    },
    "movement_control": {
        "setorigin", "setplayerangles", "setvelocity", "playerlinktoblend", "playerlinktodelta",
        "playerlinktoabsolute", "playerlinkto", "linkto", "unlink", "freezecontrols",
        "allowcrouch", "allowprone", "allowstand", "allowjump", "allowsprint", "allowmelee",
        "setstance", "setmovespeedscale", "lerpviewangleclamp", "setcandamage",
    },
    "camera_view": {
        "setviewmodel", "hideviewmodel", "showviewmodel", "getplayerangles", "geteye", "getgunangles",
        "setviewkickscale", "viewkick", "lerpfov", "getstance", "enablemousesteer",
        "disableslowaim", "enableslowaim",
    },
    "hud_ui": {
        "hidehud", "showhud", "createclientfontstring", "createclienticon", "create_client_overlay",
        "sethintstring", "sethinttext", "iprintln", "iprintlnbold",
    },
    "health_lifecycle": {
        "isalive", "kill", "suicide", "sethealth", "gethealth", "damage", "endon", "waittill",
        "notify", "enableinvulnerability", "disableinvulnerability",
    },
    "feedback_audio": {
        "playrumbleonentity", "shellshock", "playlocalsound", "stoplocalsound", "playsound",
    },
}

DEFAULT_TARGETS = {
    "black_ice": [
        ("maps/black_ice_exfil.gsc", "player_fail_finale_instakill"),
        ("maps/black_ice_ascend.gsc", "ascend_logic"),
        ("maps/black_ice_flarestack.gsc", "event_player_turns_key"),
        ("maps/black_ice_swim.gsc", "player_swim_rubberband"),
        ("maps/black_ice_util.gsc", "waittill_trigger"),
        ("maps/black_ice_util.gsc", "player_animate_underwater_damage"),
        ("maps/black_ice_command.gsc", "player_died_recently_degrades"),
        ("maps/black_ice_swim.gsc", "player_swim_think"),
        ("maps/black_ice_anim.gsc", "cw_common_breach_player"),
        ("maps/black_ice_util.gsc", "player_animated_sequence_cleanup"),
        ("maps/black_ice_anim.gsc", "notetrack_player_draw_weapon_ascend"),
        ("maps/black_ice_exfil.gsc", "magicreloadwhenreachenemy"),
        ("maps/black_ice_catwalks.gsc", "cw_tape_explode_player_effect"),
        ("maps/black_ice_exfil.gsc", "ladder_chase_explosion_fx"),
        ("maps/black_ice_swim.gsc", "snake_cam_fx"),
        ("maps/black_ice_util.gsc", "god_rays_from_world_location"),
        ("maps/black_ice_camp.gsc", "standwalktostand"),
        ("maps/black_ice_command.gsc", "disable_player"),
        ("maps/black_ice_exfil.gsc", "player_exit_to_inverted_rope"),
        ("maps/black_ice_exfil.gsc", "exfil_deck_explosions"),
        ("1062.gsc", "player_saw_kill"),
    ],
    "carrier": [
        ("maps/carrier_sparrow_run.gsc", "knockback_hesh"),
        ("maps/carrier_code_zodiac.gsc", "cut_the_rope"),
        ("maps/carrier_deck_victory.gsc", "victory_choppers_land"),
        ("maps/carrier_code.gsc", "player_slide_check"),
        ("maps/carrier_code_sparrow.gsc", "sam_missiles"),
        ("maps/carrier_code_zodiac.gsc", "can_cut_rope"),
        ("maps/carrier_deck_tilt.gsc", "tilt_player_stumble"),
        ("maps/carrier_deck_tilt.gsc", "tilt_player_slide"),
        ("maps/carrier_slow_intro.gsc", "medbay_cleanup"),
        ("maps/carrier_deck_tilt.gsc", "tilt_player_fail"),
        ("maps/carrier_slow_intro.gsc", "promote_nearest_friendly_with_classname"),
        ("maps/carrier_sparrow_run.gsc", "sparrow_run_enemy_logic"),
        ("maps/carrier_slow_intro.gsc", "setup_mantle_hint"),
        ("maps/carrier_deck_tilt.gsc", "tilt_exfil_planes"),
        ("maps/carrier_defend_sparrow.gsc", "run_deck_transition"),
        ("maps/carrier_defend_zodiac.gsc", "run_electric_trap"),
        ("maps/carrier_code.gsc", "player_anim_origin"),
        ("maps/carrier_audio.gsc", "aud_check"),
        ("maps/carrier_code.gsc", "edge_lean_natural"),
        ("maps/carrier_deck_tilt.gsc", "begin_deck_tilt"),
        ("animscripts/dog/dog_combat.gsc", "playerview_playmissanim"),
        ("1031.gsc", "monitorscavengerpickup"),
        ("maps/ss_util.gsc", "dog_monitor_goal_ent"),
    ],
    "cornered": [
        ("maps/cornered_building_entry.gsc", "gun_down_trigger_internal"),
        ("maps/cornered_destruct.gsc", "fall_player"),
        ("maps/cornered_building_entry.gsc", "upload_virus_enter"),
        ("maps/cornered_building_entry.gsc", "gun_down_trigger"),
        ("maps/cornered_audio.gsc", "aud_zipline"),
        ("maps/cornered_building_entry.gsc", "player_invasion_wipe_flag"),
        ("maps/cornered_zipline_turret.gsc", "player_handle_speed_for_knockdown"),
        ("maps/cornered_building_entry.gsc", "player_ent"),
        ("maps/cornered_building_entry.gsc", "player_in_rorkes_way"),
        ("maps/cornered_building_entry.gsc", "player_escape_door_open"),
        ("maps/cornered_destruct.gsc", "stairwell_cracks"),
        ("maps/cornered_building_entry.gsc", "handle_rappel_inverted"),
        ("maps/cornered_intro.gsc", "zipline_mvmt_npc"),
        ("maps/cornered_rappel.gsc", "player_combat_rappel"),
        ("maps/cornered_audio.gsc", "aud_rappel"),
        ("maps/cornered_building_entry.gsc", "upload_virus_loop_anims"),
        ("maps/cornered_interior.gsc", "junction_elevator_control_doors_open"),
        ("maps/cornered_intro.gsc", "intro_plane_flyby_01"),
        ("maps/cornered_audio.gsc", "aud_virus"),
        ("maps/cornered_destruct.gsc", "fall_environment"),
        ("maps/cornered_interior.gsc", "bar_enemies"),
        ("maps/cornered_interior.gsc", "courtyard_rorke"),
        ("1050.gsc", "enemy_threat_logic_dog_wait"),
        ("1045.gsc", "enemy_corpse_alert_level"),
    ],
}


class OwnershipAuditError(ValueError):
    """Raised when a mission input cannot be audited."""


def _method_category(method: str) -> str:
    lowered = method.lower()
    for category, methods in METHOD_GROUPS.items():
        if lowered in methods:
            return category
    return "other_player_api"


def _audit_function(relative: str, name: str, start: int, end: int, clean: list[str]) -> dict[str, Any]:
    body = "\n".join(clean[start - 1:end])
    player_refs = len(PLAYER_RE.findall(body))
    methods = Counter(method.lower() for method in PLAYER_METHOD_RE.findall(body))
    fields = Counter(PLAYER_FIELD_RE.findall(body))
    arrays = {key: len(pattern.findall(body)) for key, pattern in PLAYER_ARRAY_PATTERNS.items()}
    secondary_client_refs = len(SECONDARY_CLIENT_RE.findall(body))
    level_fields = Counter(LEVEL_FIELD_RE.findall(body))
    for reserved in ("player", "playerpiggyback", "playengineeffects", "players"):
        level_fields.pop(reserved, None)
    categories = Counter()
    for method, count in methods.items():
        categories[_method_category(method)] += count
    flag_calls = len(FLAG_API_RE.findall(body))
    dvar_calls = len(DVAR_API_RE.findall(body))
    side_effects = {
        "mission_failure_calls": len(MISSION_FAILURE_RE.findall(body)),
        "autosave_calls": len(AUTOSAVE_RE.findall(body)),
        "slow_motion_calls": len(SLOWMO_RE.findall(body)),
        "world_physics_calls": len(WORLD_PHYSICS_RE.findall(body)),
        "level_event_calls": len(LEVEL_EVENT_RE.findall(body)),
        "animation_rig_spawns": len(ANIM_RIG_RE.findall(body)),
    }
    level_field_writes = Counter(LEVEL_FIELD_WRITE_RE.findall(body))
    player_field_writes = Counter(PLAYER_FIELD_WRITE_RE.findall(body))
    for reserved in ("player", "playerpiggyback", "playengineeffects", "players"):
        level_field_writes.pop(reserved, None)

    event_risk_flags = []
    if side_effects["mission_failure_calls"]:
        event_risk_flags.append("MISSION_FAILURE_SIDE_EFFECT")
    if side_effects["autosave_calls"]:
        event_risk_flags.append("AUTOSAVE_SIDE_EFFECT")
    if side_effects["slow_motion_calls"]:
        event_risk_flags.append("GLOBAL_SLOW_MOTION_SIDE_EFFECT")
    if side_effects["world_physics_calls"]:
        event_risk_flags.append("WORLD_PHYSICS_SIDE_EFFECT")
    if side_effects["animation_rig_spawns"]:
        event_risk_flags.append("SCRIPTED_ANIMATION_RIGS")
    if flag_calls or side_effects["level_event_calls"]:
        event_risk_flags.append("MISSION_LEVEL_EVENT_COORDINATION")
    if dvar_calls:
        event_risk_flags.append("DVAR_SIDE_EFFECT")

    if side_effects["mission_failure_calls"] or side_effects["world_physics_calls"] or side_effects["slow_motion_calls"] or side_effects["autosave_calls"]:
        event_ownership_pattern = "MISSION_WIDE_SINGLETON_SIDE_EFFECTS"
    elif flag_calls or side_effects["level_event_calls"] or dvar_calls:
        event_ownership_pattern = "SHARED_MISSION_EVENT_WITH_PLAYER_STATE"
    elif player_refs:
        event_ownership_pattern = "PLAYER_ENTITY_ACTIONS"
    else:
        event_ownership_pattern = "NO_CLASSIFIED_EVENT_OWNERSHIP_SIGNAL"

    per_player_actions = sum(categories.values()) + sum(fields.values())
    level_scope_refs = sum(level_fields.values()) + flag_calls + dvar_calls
    if any(arrays.values()) and per_player_actions:
        ownership_pattern = "PLAYER_ARRAY_AWARE_WITH_PER_PLAYER_ACTIONS"
    elif any(arrays.values()):
        ownership_pattern = "PLAYER_ARRAY_AWARE_PATH"
    elif secondary_client_refs and per_player_actions:
        ownership_pattern = "SECONDARY_CLIENT_AND_PER_PLAYER_ACTIONS"
    elif secondary_client_refs:
        ownership_pattern = "SECONDARY_CLIENT_REFERENCE_PATH"
    elif per_player_actions and level_scope_refs:
        ownership_pattern = "MIXED_PER_PLAYER_ACTIONS_AND_LEVEL_SCOPE_STATE"
    elif per_player_actions:
        ownership_pattern = "PER_PLAYER_ACTIONS_VISIBLE"
    elif level_scope_refs:
        ownership_pattern = "LEVEL_SCOPE_STATE_VISIBLE"
    else:
        ownership_pattern = "NO_CLASSIFIED_OWNERSHIP_SIGNAL"

    return {
        "function": name,
        "file": relative,
        "start_line": start,
        "end_line": end,
        "level_player_reference_count": player_refs,
        "player_method_calls": dict(methods.most_common()),
        "player_method_categories": dict(categories.most_common()),
        "player_field_references": dict(fields.most_common()),
        "player_array_references": arrays,
        "secondary_client_reference_count": secondary_client_refs,
        "level_scope_field_references": dict(level_fields.most_common()),
        "mission_flag_api_call_count": flag_calls,
        "dvar_api_call_count": dvar_calls,
        "player_field_writes": dict(player_field_writes.most_common()),
        "level_scope_field_writes": dict(level_field_writes.most_common()),
        "shared_side_effect_counts": side_effects,
        "event_risk_flags": event_risk_flags,
        "event_ownership_pattern": event_ownership_pattern,
        "ownership_pattern": ownership_pattern,
        "warning": (
            "Static ownership hint only. Do not duplicate or replace shared flags, rigs, devices, checkpoints, "
            "or world state as if they were per-player data."
        ),
    }


def audit_mission(name: str, root: Path, target_names: list[str] | None = None) -> dict[str, Any]:
    root = Path(root).expanduser().resolve()
    if not root.is_dir():
        raise OwnershipAuditError(f"mission directory does not exist: {root}")
    files = sorted(root.rglob("*.gsc"), key=lambda item: item.as_posix().casefold())
    if not files:
        raise OwnershipAuditError(f"no GSC source files found under {root}")
    target_specs = set(DEFAULT_TARGETS.get(name, [])) if target_names is None else None
    wanted_names = set(target_names) if target_names is not None else None
    if target_specs is not None and not target_specs:
        raise OwnershipAuditError(f"no exact target functions configured for mission {name!r}")
    if wanted_names is not None and not wanted_names:
        raise OwnershipAuditError(f"no target functions configured for mission {name!r}")

    findings: list[dict[str, Any]] = []
    player_container_functions: list[dict[str, Any]] = []
    for path in files:
        original = path.read_text(encoding="utf-8", errors="replace").splitlines()
        clean = clean_source_lines(original)
        relative = path.relative_to(root).as_posix()
        for function_name, start, end in function_ranges(clean):
            target_match = (
                (relative, function_name) in target_specs
                if target_specs is not None
                else function_name in wanted_names
            )
            body = "\n".join(clean[start - 1:end])
            has_player_container = (
                any(pattern.search(body) for pattern in PLAYER_ARRAY_PATTERNS.values())
                or SECONDARY_CLIENT_RE.search(body) is not None
            )
            if target_match:
                findings.append(_audit_function(relative, function_name, start, end, clean))
            if has_player_container:
                player_container_functions.append(
                    _audit_function(relative, function_name, start, end, clean)
                )

    if target_specs is not None:
        found_specs = {(item["file"], item["function"]) for item in findings}
        missing = sorted(f"{relative}::{function}" for relative, function in target_specs - found_specs)
        target_count = len(target_specs)
    else:
        found_names = {item["function"] for item in findings}
        missing = sorted(wanted_names - found_names)
        target_count = len(wanted_names)
    return {
        "mission": name,
        "status": "PLAYER_STATE_OWNERSHIP_AUDIT_COMPLETED",
        "source_file_count": len(files),
        "target_function_count": target_count,
        "target_functions_found": len(findings),
        "missing_target_functions": missing,
        "findings": findings,
        "player_container_function_count": len(player_container_functions),
        "player_container_summary": {
            "player_array_references": {
                key: sum(item["player_array_references"][key] for item in player_container_functions)
                for key in PLAYER_ARRAY_PATTERNS
            },
            "secondary_client_reference_count": sum(
                item["secondary_client_reference_count"] for item in player_container_functions
            ),
        },
        "player_container_functions": player_container_functions,
        "interpretation": (
            "The audit distinguishes visible player entity operations from mission-level state references. "
            "A level-scoped field is listed for ownership review; its presence under level does not prove "
            "whether the underlying object is mission-global or player-specific at runtime."
        ),
    }


def audit_paths(missions: dict[str, Path]) -> dict[str, Any]:
    if not missions:
        raise OwnershipAuditError("provide at least one mission directory")
    reports = [audit_mission(name, path) for name, path in sorted(missions.items())]
    return {
        "schema_version": 1,
        "status": "CAMPAIGN_PLAYER_STATE_OWNERSHIP_AUDIT_COMPLETED",
        "mission_count": len(reports),
        "missions": reports,
        "interpretation": (
            "Static ownership hints only. This report does not implement co-op changes and does not prove "
            "a second native Campaign local client."
        ),
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("missions", nargs="+", help="NAME=PATH pairs containing decompiled .gsc files")
    parser.add_argument("--output-json", required=True, type=Path)
    args = parser.parse_args(argv)
    try:
        missions: dict[str, Path] = {}
        for item in args.missions:
            if "=" not in item:
                raise OwnershipAuditError(f"expected NAME=PATH, got {item!r}")
            name, raw_path = item.split("=", 1)
            if not name or not raw_path or name in missions:
                raise OwnershipAuditError(f"invalid or duplicate mission mapping: {item!r}")
            missions[name] = Path(raw_path)
        report = audit_paths(missions)
        args.output_json.parent.mkdir(parents=True, exist_ok=True)
        args.output_json.write_text(json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        print(json.dumps({
            "status": report["status"],
            "missions": [{
                "mission": mission["mission"],
                "target_function_count": mission["target_function_count"],
                "target_functions_found": mission["target_functions_found"],
                "ownership_patterns": dict(Counter(item["ownership_pattern"] for item in mission["findings"])),
            } for mission in report["missions"]],
        }, indent=2))
        return 0
    except (OSError, OwnershipAuditError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
