#!/usr/bin/env python3
"""Read-only source audit for the Clockwork drill player-routing patch."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from tools.campaign_player_dependency_audit import (  # noqa: E402
    METHOD_RE,
    PLAYER_RE,
    categories,
    function_ranges,
    strip_comments_and_strings,
)

CALL_RE = re.compile(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(")
SIGNATURE_RE = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*)\s*\((.*)\)\s*$")
RECEIVER_METHOD_RE = re.compile(
    r"\b([A-Za-z_][A-Za-z0-9_]*)\s+([A-Za-z_][A-Za-z0-9_]*)\s*\("
)

CORE_TARGETS = (
    "handle_drill_spot",
    "handle_drilling",
    "handle_misc_drill_details",
)
RELATED_TARGETS = (
    "watch_for_air_drop_death",
    "restore_footstep_notetrack_scripts",
    "drillthrough_plate_sound",
    "stop_current_car_watcher",
    "clockwork_drill_plant_sound",
)
PRIMARY_ONLY_TARGETS = ("switchmsg",)
COORDINATION_TARGETS = (
    "clockwork_setup_player_two",
    "handle_drill_ammo_counter",
    "clockwork_watch_player_pair",
    "new_trench_setup",
)

REFERENCE_FASTFILE_SHA256 = "a2ac70c7349047f2eded5f45a5e46ff8278689839fe1655106263520fe639783"
REFERENCE_GSC_SOURCE_SHA256 = "7fc0f02476d5c9ebbe7c28e68abe762a26a976a28587228c5027d1a5c543e1f1"
REFERENCE_FUNCTIONS = {
    "switchmsg": {"start_line": 263, "end_line": 277, "level_player_refs": 8},
    "handle_drill_spot": {"start_line": 278, "end_line": 503, "level_player_refs": 32},
    "restore_footstep_notetrack_scripts": {"start_line": 504, "end_line": 514, "level_player_refs": 4},
    "handle_drilling": {"start_line": 563, "end_line": 707, "level_player_refs": 10},
    "drillthrough_plate_sound": {"start_line": 717, "end_line": 734, "level_player_refs": 1},
    "handle_misc_drill_details": {"start_line": 760, "end_line": 848, "level_player_refs": 15},
}

SAFE_PRIMARY_ALIAS_PATTERNS = (
    re.compile(r"\bdrill_player\s*=\s*level\.player\s*;"),
    re.compile(r"\bdrill_player\s*!=\s*level\.player\b"),
)


def _function_inventory(path: Path) -> tuple[dict[str, dict], dict[str, str], dict[str, str]]:
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    ranges = function_ranges(lines)
    names = {name for name, _, _ in ranges}
    inventory: dict[str, dict] = {}
    bodies: dict[str, str] = {}
    literal_bodies: dict[str, str] = {}

    for name, start, end in ranges:
        signature = strip_comments_and_strings(lines[start - 1]).strip()
        match = SIGNATURE_RE.match(signature)
        parameters = []
        if match:
            parameters = [
                part.strip().split()[-1]
                for part in match.group(2).split(",")
                if part.strip()
            ]

        body_lines = lines[start - 1:end]
        clean_body = "\n".join(strip_comments_and_strings(line) for line in body_lines)
        direct_methods = {method.lower() for method in METHOD_RE.findall(clean_body)}
        parameter_names = set(parameters)
        for receiver, method in RECEIVER_METHOD_RE.findall(clean_body):
            if receiver in parameter_names:
                direct_methods.add(method.lower())

        calls = sorted({
            call for call in CALL_RE.findall(clean_body)
            if call in names and call != name
        })
        raw_player_refs = len(PLAYER_RE.findall(clean_body))
        allowed_primary_alias_refs = 0
        if name == "handle_drill_spot":
            allowed_primary_alias_refs = sum(
                len(pattern.findall(clean_body)) for pattern in SAFE_PRIMARY_ALIAS_PATTERNS
            )
        unsafe_player_refs = max(0, raw_player_refs - allowed_primary_alias_refs)

        row = {
            "function": name,
            "start_line": start,
            "end_line": end,
            "parameters": parameters,
            "level_player_refs": raw_player_refs,
            "allowed_primary_alias_refs": allowed_primary_alias_refs,
            "unsafe_level_player_refs": unsafe_player_refs,
            "player_methods": sorted(direct_methods),
            "player_method_categories": categories(direct_methods) if direct_methods else [],
            "local_calls": calls,
        }
        bodies[name] = clean_body
        literal_bodies[name] = re.sub(r"//[^\r\n]*|/\*.*?\*/", "", "\n".join(body_lines), flags=re.S)

        if name in inventory:
            existing = inventory[name]
            if isinstance(existing, list):
                existing.append(row)
            else:
                inventory[name] = [existing, row]
        else:
            inventory[name] = row

    return inventory, bodies, literal_bodies


def _rows_for(inventory: dict[str, dict], name: str) -> list[dict]:
    value = inventory.get(name)
    if value is None:
        return []
    return value if isinstance(value, list) else [value]


def _call_closure(inventory: dict[str, dict], root: str) -> list[str]:
    pending = [root]
    visited: set[str] = set()
    while pending:
        current = pending.pop()
        if current in visited:
            continue
        visited.add(current)
        for row in _rows_for(inventory, current):
            pending.extend(call for call in row["local_calls"] if call not in visited)
    return sorted(visited)


def _compare_reference_baseline(inventory: dict[str, dict], reference_source: Path) -> dict:
    comparisons = {}
    mismatches = []
    for name, expected in REFERENCE_FUNCTIONS.items():
        rows = _rows_for(inventory, name)
        observed = None
        if len(rows) == 1:
            row = rows[0]
            observed = {
                "start_line": row["start_line"],
                "end_line": row["end_line"],
                "level_player_refs": row["level_player_refs"],
            }
        matches = observed == expected
        comparisons[name] = {"expected": expected, "observed": observed, "matches": matches}
        if not matches:
            mismatches.append(name)

    observed_hash = hashlib.sha256(reference_source.read_bytes()).hexdigest()
    hash_matches = observed_hash == REFERENCE_GSC_SOURCE_SHA256
    return {
        "reference_fastfile_sha256": REFERENCE_FASTFILE_SHA256,
        "expected_gsc_source_sha256": REFERENCE_GSC_SOURCE_SHA256,
        "observed_gsc_source_sha256": observed_hash,
        "source_hash_matches": hash_matches,
        "function_comparisons": comparisons,
        "matches": not mismatches and hash_matches,
        "mismatches": mismatches,
    }


def _has(body: str, pattern: str) -> bool:
    return re.search(pattern, body) is not None


def audit(
    path: Path,
    *,
    reference_source: Path | None = None,
    require_reference_baseline: bool = False,
) -> dict:
    path = Path(path)
    if not path.is_file():
        raise FileNotFoundError(f"Clockwork GSC source was not found: {path}")
    if path.suffix.lower() != ".gsc":
        raise ValueError("input must be a decompiled .gsc source file")

    inventory, bodies, literal_bodies = _function_inventory(path)
    missing = [
        name for name in (*CORE_TARGETS, *RELATED_TARGETS, *COORDINATION_TARGETS)
        if not _rows_for(inventory, name)
    ]
    roots = [name for name in (*CORE_TARGETS, *RELATED_TARGETS) if _rows_for(inventory, name)]
    closures = {name: _call_closure(inventory, name) for name in roots}
    closure_functions = sorted({name for closure in closures.values() for name in closure})
    closure_rows = [
        row for name in closure_functions for row in _rows_for(inventory, name)
    ]
    unscoped_functions = sorted({
        row["function"] for row in closure_rows if row["unsafe_level_player_refs"]
    })

    def body(name: str) -> str:
        return bodies.get(name, "")

    def literal_body(name: str) -> str:
        return literal_bodies.get(name, "")

    signatures = {
        name: [row["parameters"] for row in _rows_for(inventory, name)]
        for name in (*CORE_TARGETS, *RELATED_TARGETS, *COORDINATION_TARGETS, *PRIMARY_ONLY_TARGETS)
    }

    trigger_capture = (
        _has(body("handle_drill_spot"), r"getent\s*\(\s*var_0\s*,")
        and _has(body("handle_drill_spot"), r"drill_trigger\s+waittill\s*\(")
        and _has(literal_body("handle_drill_spot"), r"waittill\s*\(\s*\"trigger\"")
    )
    activator_forwarded = (
        "drill_activator" in body("handle_drill_spot")
        and _has(body("handle_drill_spot"), r"flag_set\s*\(\s*var_1\s*,\s*drill_player\s*\)")
        and _has(body("handle_drill_spot"), r"thread\s+handle_drilling\s*\(\s*var_2\s*,\s*var_1\s*,\s*drill_player\s*\)")
    )
    native_secondary_check = (
        "issplitscreenplayer" in body("clockwork_setup_player_two")
        and "issplitscreenplayerprimary" in body("clockwork_setup_player_two")
        and "takeallweapons" in body("clockwork_setup_player_two")
    )
    secondary_loadout = all(
        token in literal_body("clockwork_setup_player_two")
        for token in (
            "gm6+scopegm6_sp+silencer03_sp",
            "cz805bren+reflex_sp+silencer_sp",
            "fraggrenade",
            "flash_grenade",
        )
    )
    pair_watcher = (
        "level.playengineeffects" in body("clockwork_watch_player_pair")
        and "clockwork_setup_player_two" in body("clockwork_watch_player_pair")
        and "handle_misc_drill_details" in body("clockwork_watch_player_pair")
    )
    ammo_counter = (
        "level.player" in body("handle_drill_ammo_counter")
        and "level.playengineeffects" in body("handle_drill_ammo_counter")
        and "ammoCounterHide" in literal_body("handle_drill_ammo_counter")
    )
    ammo_counter_restores_original = (
        _has(literal_body("handle_drill_ammo_counter"), r"getdvar\s*\(\s*\"ammoCounterHide\"")
        and _has(literal_body("handle_drill_ammo_counter"), r"setsaveddvar\s*\(\s*\"ammoCounterHide\"\s*,\s*var_0\s*\)")
    )
    player_scoped_signatures = (
        signatures.get("handle_drilling") == [["var_0", "var_1", "player"]]
        and signatures.get("handle_misc_drill_details") == [["player"]]
        and signatures.get("watch_for_air_drop_death") == [["player"]]
        and signatures.get("restore_footstep_notetrack_scripts") == [["player"]]
        and signatures.get("drillthrough_plate_sound") == [["var_0", "var_1", "player"]]
        and signatures.get("stop_current_car_watcher") == [["player", "var_0", "var_1"]]
    )
    drill_worker_does_not_block_on_start_flag = "flag_wait" not in body("handle_drilling")
    per_player_aim_state = (
        "level.aiming_at_ally" not in body("handle_misc_drill_details")
        and "level.aiming_at_ally" not in body("watch_for_air_drop_death")
        and "player.ghc_aiming_at_ally" in body("handle_misc_drill_details")
        and "player.ghc_aiming_at_ally" in body("watch_for_air_drop_death")
    )
    per_player_ammo = (
        "drill_player.ghc_clockwork_frag_ammo" in body("handle_drill_spot")
        and "drill_player.ghc_clockwork_flash_ammo" in body("handle_drill_spot")
        and "player.ghc_clockwork_frag_ammo" in body("restore_footstep_notetrack_scripts")
        and "player.ghc_clockwork_flash_ammo" in body("restore_footstep_notetrack_scripts")
    )
    death_disconnect_recovery = (
        "!isalive( drill_player )" in body("handle_drill_spot")
        and _has(body("handle_drill_spot"), r"flag_clear\s*\(\s*var_1\s*\)")
        and _has(body("handle_drill_spot"), r"drill_trigger\s+waittill\s*\(")
        and _has(body("handle_drill_spot"), r"enable_trigger_with_targetname\s*\(\s*var_0\s*\)")
        and _has(body("handle_drill_spot"), r"wait\s+0\.1")
    )
    p2_drill_press_grant = (
        "level.ghc_clockwork_drill_available = 1" in body("switchmsg")
        and "level.playengineeffects giveweapon( \"drill_press\" )" in literal_body("switchmsg")
    )
    p2_drill_press_cleanup = (
        "level.ghc_clockwork_drill_available = 0" in body("new_trench_setup")
        and "level.playengineeffects takeweapon( \"drill_press\" )" in literal_body("new_trench_setup")
    )
    primary_only_switchmsg = (
        bool(_rows_for(inventory, "switchmsg"))
        and bool(PLAYER_RE.findall(body("switchmsg")))
        and p2_drill_press_grant
    )
    drill_owner_returned_to_caller = (
        len(re.findall(r"return\\s+drill_player\\s*;", body("handle_drill_spot"))) >= 2
    )
    post_drill_animation_uses_returned_owner = (
        _has(body("new_trench_setup"), r"drill_player_1\\s*=\\s*handle_drill_spot\\s*\\(")
        and _has(body("new_trench_setup"), r"drill_player_2\\s*=\\s*handle_drill_spot\\s*\\(")
        and "drill_player_1 playerlinktodelta" in body("new_trench_setup")
        and "drill_player_2 playerlinktodelta" in body("new_trench_setup")
        and "level.player playerlinktodelta" not in body("new_trench_setup")
    )
    player_scoped_drill_plant_audio = (
        "clockwork_drill_plant_sound" in body("handle_drill_spot")
        and signatures.get("clockwork_drill_plant_sound") == [["player"]]
        and "player playsound" in body("clockwork_drill_plant_sound")
        and "level.player" not in body("clockwork_drill_plant_sound")
    )
    player_scoped_drill_pullout_audio = (
        literal_body("handle_misc_drill_details").count(
            'player playsound( "clkw_scn_vault_drill_pullout" )'
        ) >= 2
        and "maps\\\\clockwork_audio::drill_pullout" not in literal_body("handle_misc_drill_details")
    )

    checks = {
        "all_required_functions_found": not missing,
        "trigger_activator_capture_present": trigger_capture,
        "trigger_activator_passed_to_drilling": activator_forwarded,
        "native_secondary_identity_verified": native_secondary_check,
        "secondary_clockwork_loadout_present": secondary_loadout,
        "pair_watcher_starts_player_helpers": pair_watcher,
        "ammo_counter_checks_both_players": ammo_counter,
        "ammo_counter_restores_original_setting": ammo_counter_restores_original,
        "drill_helpers_use_explicit_player_arguments": player_scoped_signatures,
        "drill_worker_does_not_block_on_start_flag": drill_worker_does_not_block_on_start_flag,
        "no_unsafe_level_player_references_in_drill_call_graph": not unscoped_functions,
        "aiming_state_is_per_player": per_player_aim_state,
        "grenade_ammo_is_per_player": per_player_ammo,
        "death_disconnect_reopens_drill_trigger": death_disconnect_recovery,
        "logan_only_pickup_preserved_and_secondary_granted_drill": primary_only_switchmsg,
        "secondary_drill_press_removed_after_vault": p2_drill_press_cleanup,
        "drill_owner_returned_to_caller": drill_owner_returned_to_caller,
        "post_drill_animation_uses_returned_owner": post_drill_animation_uses_returned_owner,
        "drill_plant_audio_uses_actual_owner": player_scoped_drill_plant_audio,
        "drill_pullout_audio_uses_actual_owner": player_scoped_drill_pullout_audio,
    }

    if reference_source is not None:
        reference_source = Path(reference_source)
        if not reference_source.is_file():
            raise FileNotFoundError(f"original Clockwork reference GSC was not found: {reference_source}")
        if reference_source.suffix.lower() != ".gsc":
            raise ValueError("reference input must be a decompiled .gsc source file")
        reference_inventory, _, _ = _function_inventory(reference_source)
        reference_baseline = _compare_reference_baseline(reference_inventory, reference_source)
        reference_baseline["provided"] = True
        reference_baseline["reference_source_file"] = reference_source.name
    else:
        reference_baseline = {
            "reference_fastfile_sha256": REFERENCE_FASTFILE_SHA256,
            "expected_gsc_source_sha256": REFERENCE_GSC_SOURCE_SHA256,
            "provided": False,
            "reference_source_file": None,
            "function_comparisons": {},
            "matches": False,
            "mismatches": list(REFERENCE_FUNCTIONS),
        }

    if missing:
        status = "SOURCE_INCOMPLETE"
    elif require_reference_baseline and not reference_baseline["provided"]:
        status = "SOURCE_REFERENCE_MISSING"
    elif require_reference_baseline and not reference_baseline["matches"]:
        status = "SOURCE_BASELINE_MISMATCH"
    elif all(checks.values()):
        status = "SOURCE_ADAPTER_READY_FOR_MANUAL_REVIEW"
    else:
        status = "BLOCKED_INCOMPLETE_PLAYER_SCOPING"

    return {
        "status": status,
        "source_file": path.name,
        "required_core_functions": list(CORE_TARGETS),
        "related_functions": list(RELATED_TARGETS),
        "primary_only_functions": list(PRIMARY_ONLY_TARGETS),
        "coordination_functions": list(COORDINATION_TARGETS),
        "target_functions": {
            name: _rows_for(inventory, name) for name in CORE_TARGETS
        },
        "related_function_details": {
            name: _rows_for(inventory, name) for name in RELATED_TARGETS
        },
        "primary_only_function_details": {
            name: _rows_for(inventory, name) for name in PRIMARY_ONLY_TARGETS
        },
        "coordination_function_details": {
            name: _rows_for(inventory, name) for name in COORDINATION_TARGETS
        },
        "call_graph": closures,
        "drill_call_graph_functions": closure_functions,
        "unscoped_level_player_functions": unscoped_functions,
        "checks": checks,
        "reference_baseline": reference_baseline,
        "reference_baseline_required_for_strict_mode": require_reference_baseline,
        "limitations": [
            "This is static source analysis only; it does not load the script into the retail Campaign VM.",
            "The patch listens directly to the Clockwork trigger entity's standard trigger event to capture its toucher; the exact MapEnts trigger class and runtime event delivery still need in-game confirmation.",
            "The patch consumes the native secondary-player alias if the engine already created it; it does not create a second Campaign Local Client.",
            "The drill PIP camera is still managed through the shared level.pintoscreenedge path, whose stock initialization is tied to level.player; independent Player 2 PIP/HUD rendering remains a separate runtime/asset task.",
            "A clean source audit and successful GSC compilation do not prove independent camera/HUD rendering or playable local co-op.",
            "The compiled GSCBIN still needs to be inserted into a rebuilt Clockwork FastFile; no installable PKG is produced by this audit.",
        ],
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="patched maps__clockwork_interior.gsc")
    parser.add_argument("--json-out", type=Path, help="optional path for the JSON audit report")
    parser.add_argument(
        "--reference-source",
        type=Path,
        help="original, unmodified Clockwork GSC used to verify the pinned baseline",
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="require all routing checks plus the exact original GSC reference source",
    )
    args = parser.parse_args(argv)

    try:
        report = audit(
            args.source,
            reference_source=args.reference_source,
            require_reference_baseline=args.strict,
        )
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    rendered = json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True) + "\n"
    if args.json_out:
        try:
            if args.json_out.resolve() == args.source.resolve():
                raise ValueError("JSON report path must not overwrite the input GSC source")
            args.json_out.parent.mkdir(parents=True, exist_ok=True)
            args.json_out.write_text(rendered, encoding="utf-8")
        except (OSError, ValueError) as exc:
            print(f"error: {exc}", file=sys.stderr)
            return 1
    print(rendered, end="")
    if args.strict and report["status"] != "SOURCE_ADAPTER_READY_FOR_MANUAL_REVIEW":
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
