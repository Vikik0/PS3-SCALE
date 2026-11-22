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
    GROUPS,
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
TARGETS = ("handle_drill_spot", "handle_drilling")
ADAPTER_MARKERS = {
    "begin": "ghc_campaign_drill_begin",
    "complete": "ghc_campaign_drill_complete",
    "cancel": "ghc_campaign_drill_cancel",
    "abort_player": "ghc_campaign_drill_abort_player",
}


def _function_inventory(path: Path) -> tuple[list[str], dict[str, dict]]:
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

        body_lines = lines[start:end]
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
        # Preserve duplicate function definitions rather than silently hiding one.
        if name in inventory:
            existing = inventory[name]
            if isinstance(existing, list):
                existing.append(row)
            else:
                inventory[name] = [existing, row]
        else:
            inventory[name] = row

    return lines, inventory


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


def audit(path: Path) -> dict:
    path = Path(path)
    if not path.is_file():
        raise FileNotFoundError(f"Clockwork GSC source was not found: {path}")
    if path.suffix.lower() != ".gsc":
        raise ValueError("input must be a decompiled .gsc source file")

    _, inventory = _function_inventory(path)
    missing = [name for name in TARGETS if not _rows_for(inventory, name)]
    closures = {name: _call_closure(inventory, name) for name in TARGETS if name in inventory}

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
    for marker, function_name in ADAPTER_MARKERS.items():
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

    checks = {
        "both_drill_functions_found": not missing,
        "begin_called_from_handle_drill_spot": begin_in_entry,
        "completion_path_present_in_drill_call_graph": complete_in_closure,
        "cancellation_path_present_in_drill_call_graph": cancel_in_closure,
        "no_level_player_references_in_drill_call_graph": not global_player_functions,
        "abort_player_helper_referenced_in_source": bool(marker_locations["abort_player"]),
    }

    if missing:
        status = "SOURCE_INCOMPLETE"
    elif all(checks[key] for key in (
        "both_drill_functions_found",
        "begin_called_from_handle_drill_spot",
        "completion_path_present_in_drill_call_graph",
        "cancellation_path_present_in_drill_call_graph",
        "no_level_player_references_in_drill_call_graph",
        "abort_player_helper_referenced_in_source",
    )):
        status = "SOURCE_ADAPTER_READY_FOR_MANUAL_REVIEW"
    else:
        status = "BLOCKED_INCOMPLETE_PLAYER_SCOPING"

    target_details = {}
    for name in TARGETS:
        rows = _rows_for(inventory, name)
        target_details[name] = rows

    return {
        "status": status,
        "source_file": path.name,
        "target_functions": target_details,
        "call_graph": closures,
        "drill_call_graph_functions": closure_functions,
        "unscoped_level_player_functions": global_player_functions,
        "adapter_marker_locations": marker_locations,
        "checks": checks,
        "limitations": [
            "This is static source analysis only; it does not compile GSC or load it into the retail Campaign VM.",
            "A clean report does not prove that the passed player is the actual activating controller or that the drill spot index is mapped correctly.",
            "Weapon, ADS/fire input, stance, animation link, action-slot and control-lock cleanup still require manual path-by-path review.",
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
        help="return exit code 2 unless the drill adapter passes the static source gate",
    )
    args = parser.parse_args(argv)

    try:
        report = audit(args.source)
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
