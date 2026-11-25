#!/usr/bin/env python3
"""Audit player/activator propagation into shared mission interactions in Ghosts GSC archives."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
import zipfile
from pathlib import Path

EXPECTED_SOURCE_HASHES = {
    "black_ice_flarestack.gsc": "87257aab8fa64715abc08636768a1c52a93ffc2a1d4c8e36b0a6e4878df8c11f",
    "black_ice_util.gsc": "3de920d152d16b5c70673d322ce2f9d65e22fab5a88ba7a4c7dad3291828c355",
    "carrier_code_zodiac.gsc": "d7f7f2d9917dc2a7756cfa5ee1c4c9d7a9653b2427a4a5d61520017623feba7f",
    "cornered_building_entry.gsc": "723471a1f7aa700b982ce5b7225decd2d2e274bd88206352c083dc119bab8214",
    "player_scripted_anim_util.gsc": "40886bdad63803469e7e54bfd565ebe8fea61aa8643d2a4ec9d4be385bb76b0e",
}

SOURCE_SUFFIXES = {
    "black_ice_flarestack.gsc": "missions/black_ice/decompiled_gsc/maps/black_ice_flarestack.gsc",
    "black_ice_util.gsc": "missions/black_ice/decompiled_gsc/maps/black_ice_util.gsc",
    "carrier_code_zodiac.gsc": "missions/carrier/decompiled_gsc/maps/carrier_code_zodiac.gsc",
    "cornered_building_entry.gsc": "missions/cornered/decompiled_gsc/maps/cornered_building_entry.gsc",
    "player_scripted_anim_util.gsc": "missions/cornered/decompiled_gsc/maps/player_scripted_anim_util.gsc",
}

SHARED_INTERACTIONS = (
    {
        "mission": "Black Ice",
        "routine": "event_player_turns_key",
        "event_lock_id": 0,
        "source": "black_ice_flarestack.gsc",
        "helper_source": "black_ice_util.gsc",
        "helper": "waittill_trigger",
        "reason": "The console sequence calls a helper that polls level.player and returns no activator. The interaction routine has no player parameter.",
        "required_refactor": "Add an activator-aware console trigger path, pass the validated player into the interaction routine, and keep flare-stack flags/animation timeline under one mission coordinator.",
    },
    {
        "mission": "Carrier",
        "routine": "cut_the_rope",
        "event_lock_id": 1,
        "source": "carrier_code_zodiac.gsc",
        "helper_source": "carrier_code_zodiac.gsc",
        "helper": "wait_for_capture",
        "reason": "wait_for_capture registers melee commands on level.player; var_0 is the map struct/animation target, while self remains the rope entity. Neither value is the interacting player.",
        "required_refactor": "Pass a validated player separately from the map struct through ranktable, shoot_rocket, run_corpse_cleanup, wait_for_capture, monitor_callbutton, can_cut_rope, enemy_too_close and cut_the_rope; preserve self as the rope entity and var_0 as the map struct/animation target.",
    },
    {
        "mission": "Cornered",
        "routine": "gun_down_trigger_internal",
        "event_lock_id": 2,
        "source": "cornered_building_entry.gsc",
        "helper_source": "cornered_building_entry.gsc",
        "helper": "gun_down_trigger",
        "reason": "The handler is started by a global flag and has no activator parameter; weapon snapshots and weapon methods all reference level.player, while ammoCounterHide is a shared dvar.",
        "required_refactor": "Capture the actual trigger player, pass it into gun_down_trigger_internal, move weapon snapshots to that player, and serialize the shared ammoCounterHide dvar through one coordinator.",
    },
    {
        "mission": "Cornered",
        "routine": "upload_virus_enter",
        "event_lock_id": 3,
        "source": "cornered_building_entry.gsc",
        "helper_source": "player_scripted_anim_util.gsc",
        "helper": "waittill_trigger",
        "reason": "The upstream trigger helper polls level.player; upload progress/exit/enter handoffs use global flags and the upload handler has no player parameter.",
        "required_refactor": "Propagate the validated activator through the upload trigger, upload_progress, upload_virus_exit and upload_virus_enter; retain one owner for the shared device, flags, dynamic DOF and allow-fire state.",
    },
)

MISSION_COORDINATORS = (
    ("Black Ice", "player_fail_finale_instakill", "black_ice_exfil.gsc"),
    ("Black Ice", "ascend_logic", "black_ice_ascend.gsc"),
    ("Carrier", "knockback_hesh", "carrier_sparrow_run.gsc"),
    ("Carrier", "victory_choppers_land", "carrier_deck_victory.gsc"),
    ("Cornered", "fall_player", "cornered_destruct.gsc"),
)

PLAYER_PARAMETER_NAMES = {
    "player", "activator", "interacting_player", "trigger_player",
    "local_player", "client_player", "user",
}
RESERVED_WORDS = {"if", "for", "while", "switch", "foreach", "else", "do"}


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _mask_comments_and_strings(source: str) -> str:
    """Replace comments and quoted text with spaces while preserving offsets/newlines."""
    out = list(source)
    index = 0
    quote = None
    line_comment = False
    block_comment = False
    escaped = False
    while index < len(source):
        char = source[index]
        next_char = source[index + 1] if index + 1 < len(source) else ""
        if line_comment:
            if char == "\n":
                line_comment = False
            else:
                out[index] = " "
            index += 1
            continue
        if block_comment:
            if char == "*" and next_char == "/":
                out[index] = out[index + 1] = " "
                block_comment = False
                index += 2
            else:
                if char != "\n":
                    out[index] = " "
                index += 1
            continue
        if quote:
            if char != "\n":
                out[index] = " "
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = None
            index += 1
            continue
        if char == "/" and next_char == "/":
            out[index] = out[index + 1] = " "
            line_comment = True
            index += 2
            continue
        if char == "/" and next_char == "*":
            out[index] = out[index + 1] = " "
            block_comment = True
            index += 2
            continue
        if char in ('"', "'"):
            out[index] = " "
            quote = char
        index += 1
    return "".join(out)


def _function_ranges(source: str) -> list[dict]:
    masked = _mask_comments_and_strings(source)
    signature_re = re.compile(r"(?m)^([A-Za-z_][A-Za-z0-9_]*)\s*\(([^)]*)\)\s*\{")
    functions = []
    for match in signature_re.finditer(masked):
        name = match.group(1)
        if name.lower() in RESERVED_WORDS:
            continue
        opening = masked.find("{", match.start(), match.end())
        depth = 0
        closing = None
        for index in range(opening, len(masked)):
            if masked[index] == "{":
                depth += 1
            elif masked[index] == "}":
                depth -= 1
                if depth == 0:
                    closing = index
                    break
        if closing is None:
            continue
        params = [part.strip().split()[-1] for part in match.group(2).split(",") if part.strip()]
        functions.append({
            "name": name,
            "params": params,
            "start": match.start(),
            "end": closing + 1,
            "body": masked[opening + 1:closing],
            "raw_body": source[opening + 1:closing],
            "line": source.count("\n", 0, match.start()) + 1,
        })
    return functions


def _split_top_level(value: str) -> list[str]:
    parts = []
    start = 0
    depth = 0
    for index, char in enumerate(value):
        if char in "([{":
            depth += 1
        elif char in ")]}":
            depth = max(0, depth - 1)
        elif char == "," and depth == 0:
            parts.append(value[start:index])
            start = index + 1
    parts.append(value[start:])
    return parts


def _call_arguments(masked: str, opening_paren: int) -> list[str]:
    depth = 0
    start = opening_paren + 1
    for index in range(opening_paren, len(masked)):
        char = masked[index]
        if char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
            if depth == 0:
                content = masked[start:index].strip()
                return [part.strip() for part in _split_top_level(content)] if content else []
    return []


def _find_function(functions: list[dict], name: str) -> dict | None:
    matches = [item for item in functions if item["name"] == name]
    return matches[0] if len(matches) == 1 else None


def _call_sites(name: str, source_name: str, source: str) -> list[dict]:
    masked = _mask_comments_and_strings(source)
    functions = _function_ranges(source)
    definitions = {item["start"] for item in functions if item["name"] == name}
    sites = []
    for match in re.finditer(r"\b" + re.escape(name) + r"\s*\(", masked):
        if match.start() in definitions:
            continue
        opening = masked.find("(", match.start(), match.end())
        caller = next(
            (item for item in reversed(functions) if item["start"] < match.start() < item["end"]),
            None,
        )
        sites.append({
            "source": source_name,
            "line": source.count("\n", 0, match.start()) + 1,
            "caller": caller["name"] if caller else "<top-level>",
            "arguments": _call_arguments(masked, opening),
        })
    return sites


def _load_archive_sources(archive_path: Path) -> tuple[dict[str, str], dict[str, str], dict[str, str]]:
    contents = {}
    hashes = {}
    member_names = {}
    with zipfile.ZipFile(archive_path) as archive:
        bad_member = archive.testzip()
        if bad_member:
            raise ValueError(f"archive CRC check failed at {bad_member}")
        for short_name, suffix in SOURCE_SUFFIXES.items():
            matches = [name for name in archive.namelist() if name.endswith(suffix)]
            if len(matches) != 1:
                raise ValueError(f"expected exactly one archive member ending in {suffix!r}, found {len(matches)}")
            raw = archive.read(matches[0])
            contents[short_name] = raw.decode("utf-8", errors="strict")
            hashes[short_name] = _sha256(raw)
            member_names[short_name] = matches[0]
        for mission, routine, filename in MISSION_COORDINATORS:
            matches = [name for name in archive.namelist() if name.endswith("/" + filename)]
            if len(matches) != 1:
                raise ValueError(f"expected exactly one source for mission coordinator {routine}, found {len(matches)}")
            raw = archive.read(matches[0])
            contents[filename] = raw.decode("utf-8", errors="strict")
            hashes[filename] = _sha256(raw)
            member_names[filename] = matches[0]
    return contents, hashes, member_names


def audit_archive(archive_path: Path, *, enforce_reference_hashes: bool = True) -> dict:
    archive_path = Path(archive_path)
    if not archive_path.is_file():
        raise FileNotFoundError(f"mission source archive not found: {archive_path}")
    archive_bytes = archive_path.read_bytes()
    archive_hash = _sha256(archive_bytes)
    sources, hashes, member_names = _load_archive_sources(archive_path)

    hash_checks = {
        name: {
            "archive_member": member_names[name],
            "observed_sha256": hashes[name],
            "expected_sha256": EXPECTED_SOURCE_HASHES[name],
            "matches": hashes[name] == EXPECTED_SOURCE_HASHES[name],
        }
        for name in EXPECTED_SOURCE_HASHES
    }
    if enforce_reference_hashes:
        mismatches = [name for name, item in hash_checks.items() if not item["matches"]]
        if mismatches:
            raise ValueError("reference source hash mismatch: " + ", ".join(mismatches))

    shared = []
    for spec in SHARED_INTERACTIONS:
        source = sources[spec["source"]]
        functions = _function_ranges(source)
        target = _find_function(functions, spec["routine"])
        if target is None:
            raise ValueError(f"expected one function definition for {spec['routine']} in {spec['source']}")
        helper_source = sources[spec["helper_source"]]
        helper = _find_function(_function_ranges(helper_source), spec["helper"])
        if helper is None:
            raise ValueError(f"expected one helper definition for {spec['helper']} in {spec['helper_source']}")
        calls = _call_sites(spec["routine"], member_names[spec["source"]], source)
        param_names = {name.lower() for name in target["params"]}
        has_player_parameter = bool(param_names & PLAYER_PARAMETER_NAMES)
        trigger_capture = bool(re.search(r"\bwaittill\s*\(\s*\"trigger\"\s*,\s*[A-Za-z_]\w*", target["raw_body"]))
        target_level_player_refs = len(re.findall(r"\blevel\s*\.\s*player\b", target["body"]))
        helper_level_player_refs = len(re.findall(r"\blevel\s*\.\s*player\b", helper["body"]))
        helper_trigger_capture = bool(re.search(r"\bwaittill\s*\(\s*\"trigger\"\s*,\s*[A-Za-z_]\w*", helper["raw_body"]))
        helper_param_names = {name.lower() for name in helper["params"]}
        helper_has_player_parameter = bool(helper_param_names & PLAYER_PARAMETER_NAMES)
        helper_returns_activator = bool(re.search(r"\breturn\s+(?:player|activator|trigger_player|local_player)\s*;", helper["raw_body"], re.I))
        explicit_context = has_player_parameter or trigger_capture or (helper_trigger_capture and helper_returns_activator)
        callsite_player_args = [
            site for site in calls
            if any(re.search(r"\b(?:player|activator|trigger_player|local_player)\b", arg, re.I) for arg in site["arguments"])
        ]
        callsites_propagate_player = bool(calls) and len(callsite_player_args) == len(calls)
        target_is_player_scoped = target_level_player_refs == 0
        helper_is_player_scoped = helper_level_player_refs == 0 or helper_has_player_parameter
        safe = explicit_context and callsites_propagate_player and target_is_player_scoped and helper_is_player_scoped
        shared.append({
            "mission": spec["mission"],
            "routine": spec["routine"],
            "event_lock_id": spec["event_lock_id"],
            "source": spec["source"],
            "source_sha256": hashes[spec["source"]],
            "definition_line": target["line"],
            "parameters": target["params"],
            "call_sites": calls,
            "target_level_player_references": target_level_player_refs,
            "target_captures_trigger_activator": trigger_capture,
            "helper": spec["helper"],
            "helper_source": spec["helper_source"],
            "helper_parameters": helper["params"],
            "helper_level_player_references": helper_level_player_refs,
            "helper_captures_trigger_activator": helper_trigger_capture,
            "helper_has_player_parameter": helper_has_player_parameter,
            "helper_returns_activator": helper_returns_activator,
            "has_explicit_player_context": explicit_context,
            "call_sites_pass_player_argument": callsites_propagate_player,
            "target_is_player_scoped": target_is_player_scoped,
            "helper_is_player_scoped": helper_is_player_scoped,
            "safe_to_wire_event_lock": safe,
            "status": "READY_FOR_PLAYER_LOCK_PATCH" if safe else "BLOCKED_ACTIVATOR_LINEAGE_MISSING",
            "reason": spec["reason"],
            "required_refactor": spec["required_refactor"],
        })

    coordinators = []
    for mission, routine, filename in MISSION_COORDINATORS:
        source = sources[filename]
        target = _find_function(_function_ranges(source), routine)
        if target is None:
            raise ValueError(f"mission coordinator function not found: {routine}")
        coordinators.append({
            "mission": mission,
            "routine": routine,
            "source": member_names[filename],
            "source_sha256": hashes[filename],
            "definition_line": target["line"],
            "parameters": target["params"],
            "level_player_references": len(re.findall(r"\blevel\s*\.\s*player\b", target["body"])),
            "event_lock_id": None,
            "policy": "MISSION_COORDINATOR_ONLY",
            "must_remain_single_shot": True,
        })

    blocked = [item for item in shared if not item["safe_to_wire_event_lock"]]
    ready = [item for item in shared if item["safe_to_wire_event_lock"]]
    return {
        "schema_version": 1,
        "phase": 89,
        "status": "ACTIVATOR_LINEAGE_AUDIT_BLOCKED_SAFE_MISSION_PATCH",
        "input_archive": archive_path.name,
        "input_archive_sha256": archive_hash,
        "source_hashes": hash_checks,
        "shared_interactions": shared,
        "mission_coordinators": coordinators,
        "summary": {
            "shared_interactions": len(shared),
            "safe_to_wire": len(ready),
            "blocked_missing_activator_lineage": len(blocked),
            "mission_coordinators_single_shot": len(coordinators),
        },
        "decision": "Do not insert Event Lock calls into original mission handlers until each trigger path propagates a validated native player entity. The current source paths are primary-player/global-flag driven; wiring a lock around level.player would serialize Player 1 only and falsely imply Player 2 support.",
        "next_gate": "Capture native Local Client 1 plus per-player input on the actual TU1.16 runtime, then refactor each trigger to pass its real activator through every player-owned handler before adding acquire/release/abort.",
        "limitations": [
            "This is static analysis of the user-supplied decompiled GSC archive.",
            "No original mission GSC source was modified by this audit.",
            "No FastFile was rebuilt or injected.",
            "No physical PS3 runtime evidence is included.",
        ],
    }


def validate_report(data: dict, *, require_reference_hashes: bool = True) -> list[str]:
    errors = []
    if data.get("schema_version") != 1 or data.get("phase") != 89:
        errors.append("report schema_version/phase must be 1/89")
    if data.get("status") != "ACTIVATOR_LINEAGE_AUDIT_BLOCKED_SAFE_MISSION_PATCH":
        errors.append("report status must preserve the activator-lineage safety gate")
    shared = data.get("shared_interactions")
    if not isinstance(shared, list) or len(shared) != 4:
        errors.append("report must contain exactly four shared interactions")
        shared = []
    ids = [item.get("event_lock_id") for item in shared]
    if sorted(ids) != [0, 1, 2, 3]:
        errors.append("shared interaction event IDs must remain 0..3")
    if any(item.get("safe_to_wire_event_lock") is not False for item in shared):
        errors.append("all four current interactions must not claim an unsafe interaction is ready to wire")
    if any(item.get("status") != "BLOCKED_ACTIVATOR_LINEAGE_MISSING" for item in shared):
        errors.append("all four current interactions must remain blocked pending activator propagation")
    coordinators = data.get("mission_coordinators")
    if not isinstance(coordinators, list) or len(coordinators) != 5:
        errors.append("report must contain exactly five mission coordinators")
        coordinators = []
    if any(item.get("policy") != "MISSION_COORDINATOR_ONLY" or item.get("event_lock_id") is not None for item in coordinators):
        errors.append("mission coordinator events must remain single-shot and have no player Event Lock ID")
    hashes = data.get("source_hashes")
    if not isinstance(hashes, dict) or set(hashes) != set(EXPECTED_SOURCE_HASHES):
        errors.append("report must contain hashes for all five trigger/interaction source files")
    else:
        for name, expected in EXPECTED_SOURCE_HASHES.items():
            item = hashes[name]
            if item.get("expected_sha256") != expected:
                errors.append(f"report expected source hash is not pinned to the verified archive: {name}")
            observed = item.get("observed_sha256", "")
            if not re.fullmatch(r"[0-9a-f]{64}", observed):
                errors.append(f"report observed source hash is invalid: {name}")
            if item.get("matches") is not (observed == expected):
                errors.append(f"report source hash match flag is inconsistent: {name}")
            if require_reference_hashes and item.get("matches") is not True:
                errors.append(f"report source hash is not pinned to the verified archive: {name}")
    summary = data.get("summary", {})
    if summary.get("safe_to_wire") != 0 or summary.get("blocked_missing_activator_lineage") != 4:
        errors.append("report summary must show 0 safe and 4 blocked interactions")
    return errors


def validate_dependency_graph(data: dict) -> list[str]:
    errors = []
    if data.get("schema_version") != 1 or data.get("phase") != 90:
        errors.append("dependency graph schema_version/phase must be 1/90")
    if data.get("status") != "REFRACTOR_DEPENDENCIES_MAPPED_PATCHES_BLOCKED_PENDING_NATIVE_PLAYER":
        errors.append("dependency graph must preserve the native-player patch gate")
    if not re.fullmatch(r"[0-9a-f]{64}", data.get("input_archive_sha256", "")):
        errors.append("dependency graph input archive SHA-256 is invalid")

    hashes = data.get("source_hashes")
    if not isinstance(hashes, dict) or set(hashes) != set(EXPECTED_SOURCE_HASHES):
        errors.append("dependency graph must pin all five trigger/helper source files")
    else:
        for name, expected in EXPECTED_SOURCE_HASHES.items():
            if hashes[name].get("sha256") != expected:
                errors.append(f"dependency graph source hash mismatch: {name}")

    nodes = data.get("nodes")
    if not isinstance(nodes, list) or not nodes:
        errors.append("dependency graph must contain function nodes")
        nodes = []
    node_ids = [item.get("id") for item in nodes if isinstance(item, dict)]
    if len(node_ids) != len(nodes) or len(set(node_ids)) != len(node_ids):
        errors.append("dependency graph node IDs must be unique and every node must be an object")
    node_id_set = set(node_ids)
    for node in nodes:
        if not isinstance(node, dict):
            continue
        if node.get("source") not in EXPECTED_SOURCE_HASHES:
            errors.append(f"dependency graph node has an unknown source: {node.get('id')}")
        if not isinstance(node.get("line"), int) or node["line"] < 1:
            errors.append(f"dependency graph node has an invalid line: {node.get('id')}")
        if not isinstance(node.get("level_player_references"), int) or node["level_player_references"] < 0:
            errors.append(f"dependency graph node has an invalid player-reference count: {node.get('id')}")

    chains = data.get("chains")
    expected_chains = {
        "black_ice_flarestack": (0, "Black Ice"),
        "carrier_rope_cut": (1, "Carrier"),
        "cornered_weapon_hide": (2, "Cornered"),
        "cornered_virus_upload": (3, "Cornered"),
    }
    if not isinstance(chains, list) or len(chains) != 4:
        errors.append("dependency graph must contain exactly four shared-interaction chains")
        chains = []
    seen_chains = set()
    chain_edge_count = 0
    for chain in chains:
        if not isinstance(chain, dict):
            errors.append("dependency graph chain entries must be objects")
            continue
        chain_id = chain.get("id")
        if chain_id in seen_chains:
            errors.append(f"duplicate dependency chain: {chain_id}")
        seen_chains.add(chain_id)
        expected = expected_chains.get(chain_id)
        if expected is None or chain.get("event_lock_id") != expected[0] or chain.get("mission") != expected[1]:
            errors.append(f"dependency chain mission/Event ID mismatch: {chain_id}")
        chain_nodes = chain.get("node_ids")
        if not isinstance(chain_nodes, list) or not chain_nodes:
            errors.append(f"dependency chain has no node IDs: {chain_id}")
            chain_nodes = []
        if any(node_id not in node_id_set for node_id in chain_nodes):
            errors.append(f"dependency chain references a missing function node: {chain_id}")
        chain_edges = chain.get("edges")
        if not isinstance(chain_edges, list):
            errors.append(f"dependency chain edges must be a list: {chain_id}")
            chain_edges = []
        chain_edge_count += len(chain_edges)
        for edge in chain_edges:
            if not isinstance(edge, dict):
                errors.append(f"dependency chain contains a malformed edge: {chain_id}")
                continue
            if edge.get("from") not in chain_nodes or edge.get("to") not in chain_nodes:
                errors.append(f"dependency edge escapes its chain: {chain_id}")
            if not isinstance(edge.get("line"), int) or edge["line"] < 1:
                errors.append(f"dependency edge has an invalid source line: {chain_id}")
            if not isinstance(edge.get("arguments"), list):
                errors.append(f"dependency edge arguments must be a list: {chain_id}")
    if seen_chains != set(expected_chains):
        errors.append("dependency graph chain IDs must be exactly 0..3 map")
    summary = data.get("summary", {})
    if summary.get("chains") != 4 or summary.get("nodes") != len(nodes):
        errors.append("dependency graph summary chain/node counts are inconsistent")
    if summary.get("edges") != summary.get("unique_edges") or summary.get("chain_edge_references") != chain_edge_count:
        errors.append("dependency graph summary edge counts are inconsistent")
    if summary.get("shared_interactions") != 4 or summary.get("mission_coordinators_single_shot") != 5:
        errors.append("dependency graph summary must preserve four interactions and five coordinators")

    carrier = data.get("carrier_entity_contract", {})
    if "map struct" not in carrier.get("ranktable_var_0", "").lower():
        errors.append("Carrier ranktable var_0 must remain the map struct")
    if "rope entity" not in carrier.get("cut_the_rope_self", "").lower():
        errors.append("Carrier cut_the_rope self must remain the rope entity")
    if "map struct" not in carrier.get("cut_the_rope_var_0", "").lower():
        errors.append("Carrier cut_the_rope var_0 must remain the map struct")
    if set(carrier.get("do_not_conflate", [])) != {"self rope entity", "var_0 map struct/animation target", "player entity"}:
        errors.append("Carrier entity contract must keep self, var_0 and player separate")

    helper_warning = data.get("shared_helper_warning", {})
    if helper_warning.get("black_ice_util_waittill_trigger_callers") != 3:
        errors.append("Black Ice shared trigger helper caller count must remain pinned at three")
    if helper_warning.get("player_scripted_anim_util_waittill_trigger_callers") != 15:
        errors.append("player scripted animation helper caller count must remain pinned at fifteen")
    if "do not globally rewrite" not in helper_warning.get("rule", "").lower():
        errors.append("shared generic trigger helpers must not be globally rewritten")

    coordinators = data.get("mission_coordinator_rules")
    if not isinstance(coordinators, list) or len(coordinators) != 5:
        errors.append("dependency graph must preserve five mission coordinator rules")
    elif any(item.get("event_lock_id") is not None or item.get("policy") != "MISSION_COORDINATOR_ONLY" for item in coordinators):
        errors.append("mission coordinator rules must remain single-shot without player Event Lock IDs")

    if "do not patch the original handlers yet" not in data.get("decision", "").lower():
        errors.append("dependency graph must not claim original handlers are patched")
    return errors



def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, required=True, help="path to Ghosts_Campaign_Missions_Phase31.zip")
    parser.add_argument("--json-out", type=Path, required=True, help="output JSON report path")
    parser.add_argument("--allow-nonreference", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args(argv)
    archive_path = args.archive
    output_path = args.json_out
    try:
        report = audit_archive(archive_path, enforce_reference_hashes=not args.allow_nonreference)
        errors = validate_report(report)
        if errors:
            raise ValueError("; ".join(errors))
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    except (OSError, ValueError, zipfile.BadZipFile, UnicodeDecodeError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1
    print("PASS: original mission trigger/activator lineage audited")
    print(f"shared interactions: {report['summary']['shared_interactions']}")
    print(f"safe to wire: {report['summary']['safe_to_wire']}")
    print(f"blocked pending activator propagation: {report['summary']['blocked_missing_activator_lineage']}")
    print(f"mission coordinators kept single-shot: {report['summary']['mission_coordinators_single_shot']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
