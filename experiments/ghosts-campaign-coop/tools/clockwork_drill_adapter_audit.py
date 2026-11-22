#!/usr/bin/env python3
"""Read-only source audit for the Clockwork drill co-op adapter."""
from __future__ import annotations

import argparse
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

# These three functions form the required drill interaction path.
CORE_TARGETS = (
    "handle_drill_spot",
    "handle_drilling",
    "handle_misc_drill_details",
)

# These adjacent functions own weapon switching, post-drill restoration and audio.
RELATED_TARGETS = (
    "switchmsg",
    "restore_footstep_notetrack_scripts",
    "drillthrough_plate_sound",
)

ADAPTER_MARKERS = {
    "begin": "ghc_campaign_drill_begin",
    "complete": "ghc_campaign_drill_complete",
    "cancel": "ghc_campaign_drill_cancel",
    "abort_player": "ghc_campaign_drill_abort_player",
}

# Reference values from the user's Clockwork asset analysis matrix.
# They are an identity/safety check, not guessed patch offsets.
REFERENCE_FASTFILE_SHA256 = "a2ac70c7349047f2eded5f45a5e46ff8278689839fe1655106263520fe639783"
REFERENCE_FUNCTIONS = {
    "switchmsg": {"start_line": 263, "end_line": 277, "level_player_refs": 8},
    "handle_drill_spot": {"start_line": 278, "end_line": 503, "level_player_refs": 32},
    "restore_footstep_notetrack_scripts": {"start_line": 504, "end_line": 514, "level_player_refs": 4},
    "handle_drilling": {"start_line": 563, "end_line": 707, "level_player_refs": 10},
    "drillthrough_plate_sound": {"start_line": 717, "end_line": 734, "level_player_refs": 1},
    "handle_misc_drill_details": {"start_line": 760, "end_line": 848, "level_player_refs": 15},
}


def _function_inventory(path: Path) -> dict[str, dict]:
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    ranges = function_ranges(lines)
    names = {name for name, _, _ in ranges}
    inventory: dict[str, dict] = {}

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
        adapter_calls = {
            marker: ADAPTER_MARKERS[marker] in clean_body
            for marker in ADAPTER_MARKERS
        }
        row = {
            "function": name,
            "start_line": start,
            "end_line": end,
            "parameters": parameters,
            "level_player_refs": len(PLAYER_RE.findall(clean_body)),
            "player_methods": sorted(direct_methods),
            "player_method_categories": categories(direct_methods) if direct_methods else [],
            "local_calls": calls,
            "adapter_calls": adapter_calls,
        }
        # Preserve duplicate definitions rather than silently hiding one.
        if name in inventory:
            existing = inventory[name]
            if isinstance(existing, list):
                existing.append(row)
            else:
                inventory[name] = [existing, row]
        else:
            inventory[name] = row

    return inventory


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


def _compare_reference_baseline(inventory: dict[str, dict]) -> dict:
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
        comparisons[name] = {
            "expected": expected,
            "observed": observed,
            "matches": matches,
        }
        if not matches:
            mismatches.append(name)
    return {
        "reference_fastfile_sha256": REFERENCE_FASTFILE_SHA256,
        "function_comparisons": comparisons,
        "matches": not mismatches,
        "mismatches": mismatches,
    }


def audit(path: Path, *, require_reference_baseline: bool = False) -> dict:
    path = Path(path)
    if not path.is_file():
        raise FileNotFoundError(f"Clockwork GSC source was not found: {path}")
    if path.suffix.lower() != ".gsc":
        raise ValueError("input must be a decompiled .gsc source file")

    inventory = _function_inventory(path)
    missing = [name for name in CORE_TARGETS if not _rows_for(inventory, name)]
    roots = [name for name in (*CORE_TARGETS, *RELATED_TARGETS) if _rows_for(inventory, name)]
    closures = {name: _call_closure(inventory, name) for name in roots}
    closure_functions = sorted({name for closure in closures.values() for name in closure})
    closure_rows = [
        row
        for name in closure_functions
        for row in _rows_for(inventory, name)
    ]
    global_player_functions = sorted({
        row["function"] for row in closure_rows if row["level_player_refs"]
    })

    marker_locations = {}
    for marker in ADAPTER_MARKERS:
        marker_locations[marker] = sorted({
            row["function"]
            for rows in inventory.values()
            for row in (rows if isinstance(rows, list) else [rows])
            if row["adapter_calls"][marker]
        })

    drill_spot_rows = _rows_for(inventory, "handle_drill_spot")
    begin_in_entry = any(row["adapter_calls"]["begin"] for row in drill_spot_rows)
    complete_in_closure = any(row["adapter_calls"]["complete"] for row in closure_rows)
    cancel_in_closure = any(row["adapter_calls"]["cancel"] for row in closure_rows)
    reference_baseline = _compare_reference_baseline(inventory)

    checks = {
        "all_core_drill_functions_found": not missing,
        "begin_called_from_handle_drill_spot": begin_in_entry,
        "completion_path_present_in_drill_call_graph": complete_in_closure,
        "cancellation_path_present_in_drill_call_graph": cancel_in_closure,
        "no_level_player_references_in_drill_call_graph": not global_player_functions,
        "abort_player_helper_referenced_in_source": bool(marker_locations["abort_player"]),
    }

    if missing:
        status = "SOURCE_INCOMPLETE"
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
        "target_functions": {
            name: _rows_for(inventory, name) for name in CORE_TARGETS
        },
        "related_function_details": {
            name: _rows_for(inventory, name) for name in RELATED_TARGETS
        },
        "call_graph": closures,
        "drill_call_graph_functions": closure_functions,
        "unscoped_level_player_functions": global_player_functions,
        "adapter_marker_locations": marker_locations,
        "checks": checks,
        "reference_baseline": reference_baseline,
        "reference_baseline_required_for_strict_mode": require_reference_baseline,
        "limitations": [
            "The reference baseline comes from the user's Clockwork FastFile analysis; --strict requires all six known function ranges and level.player reference counts to match it.",
            "This is static source analysis only; it does not compile GSC or load it into the retail Campaign VM.",
            "A clean report does not prove that the passed player is the actual activating controller or that the drill spot index is mapped correctly.",
            "Weapon, ADS/fire input, stance, animation link, action-slot and control-lock cleanup still require manual path-by-path review.",
            "The abort-player call site must be manually verified to run on death, disconnect, checkpoint restart and normal cancellation.",
            "Shared objective completion must remain idempotent and must not be committed by cancellation, death or disconnect.",
            "A source-ready result is not evidence of native Campaign Local Client creation, controller routing, camera/HUD separation or playable co-op.",
        ],
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="user-owned decompiled maps__clockwork_interior.gsc")
    parser.add_argument("--json-out", type=Path, help="optional path for the JSON audit report")
    parser.add_argument(
        "--strict",
        action="store_true",
        help="require both the adapter checks and the exact six-function reference baseline",
    )
    args = parser.parse_args(argv)

    try:
        report = audit(args.source, require_reference_baseline=args.strict)
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
