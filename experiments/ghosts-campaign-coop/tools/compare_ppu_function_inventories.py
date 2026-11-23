#!/usr/bin/env python3
"""Compare Ghidra function inventories and their resolved internal call graphs.

The inventories are exported separately by ExportGhostsPpuFunctionInventory.java.
This tool ranks cross-build code-fingerprint matches and Multiplayer-only
function leads. It never emits a hook address or claims semantic equivalence.
"""
from __future__ import annotations

import argparse
import csv
import json
import re
import sys
from collections import defaultdict
from pathlib import Path
from typing import Any

REQUIRED_COLUMNS = {
    "program",
    "name",
    "entry",
    "body_address_count",
    "instruction_count",
    "fingerprint",
    "call_targets",
}
DEFAULT_KEYWORDS = (
    "split", "local", "client", "controller", "player", "view",
    "camera", "viewport", "hud", "connect", "spawn", "input",
)
FINGERPRINT_RE = re.compile(r"^[0-9a-fA-F]{64}$")


class InventoryError(ValueError):
    """Raised when a Ghidra inventory is incomplete or malformed."""


def _values(value: Any) -> list[str]:
    if value is None:
        return []
    if isinstance(value, str):
        return [part.strip() for part in value.split(";") if part.strip()]
    return [str(part).strip() for part in value if str(part).strip()]


def _keyword_hits(value: str, keywords: tuple[str, ...]) -> list[str]:
    lowered = value.lower()
    return [keyword for keyword in keywords if keyword in lowered]


def read_inventory(path: Path) -> list[dict[str, Any]]:
    path = Path(path)
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        columns = set(reader.fieldnames or ())
        missing = REQUIRED_COLUMNS - columns
        if missing:
            raise InventoryError(
                f"{path}: missing required columns: {', '.join(sorted(missing))}"
            )

        rows: list[dict[str, Any]] = []
        seen_entries: set[str] = set()
        for line_number, row in enumerate(reader, 2):
            fingerprint = (row.get("fingerprint") or "").strip().lower()
            if not FINGERPRINT_RE.fullmatch(fingerprint):
                raise InventoryError(
                    f"{path}:{line_number}: fingerprint must be a 64-character SHA-256 hex string"
                )
            try:
                body_count = int(row["body_address_count"])
                instruction_count = int(row["instruction_count"])
            except (TypeError, ValueError) as exc:
                raise InventoryError(
                    f"{path}:{line_number}: body and instruction counts must be integers"
                ) from exc
            if body_count <= 0 or instruction_count <= 0:
                raise InventoryError(
                    f"{path}:{line_number}: function counts must be positive"
                )
            name = (row.get("name") or "").strip()
            entry = (row.get("entry") or "").strip()
            if not name or not entry:
                raise InventoryError(
                    f"{path}:{line_number}: function name and entry are required"
                )
            if entry in seen_entries:
                raise InventoryError(
                    f"{path}:{line_number}: duplicate function entry {entry}"
                )
            seen_entries.add(entry)
            rows.append({
                "program": (row.get("program") or "").strip(),
                "name": name,
                "entry": entry,
                "body_address_count": body_count,
                "instruction_count": instruction_count,
                "fingerprint": fingerprint,
                "call_targets": _values(row.get("call_targets")),
                "call_target_symbols": _values(row.get("call_target_symbols")),
                "call_target_entries": _values(row.get("call_target_entries")),
            })
    if not rows:
        raise InventoryError(f"{path}: inventory contains no function rows")
    return rows


def _group(rows: list[dict[str, Any]]) -> dict[str, list[dict[str, Any]]]:
    grouped: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for row in rows:
        grouped[row["fingerprint"]].append(row)
    for values in grouped.values():
        values.sort(key=lambda item: (item["entry"], item["name"]))
    return dict(grouped)


def _entry_map(rows: list[dict[str, Any]]) -> dict[str, dict[str, Any]]:
    return {row["entry"]: row for row in rows}


def _target_functions(
    row: dict[str, Any],
    functions_by_entry: dict[str, dict[str, Any]],
) -> list[dict[str, Any]]:
    targets = {
        entry: functions_by_entry[entry]
        for entry in _values(row.get("call_target_entries"))
        if entry in functions_by_entry
    }
    return sorted(
        targets.values(),
        key=lambda item: (item["fingerprint"], item["entry"], item["name"]),
    )


def _target_fingerprints(
    row: dict[str, Any],
    functions_by_entry: dict[str, dict[str, Any]],
) -> set[str]:
    return {
        target["fingerprint"]
        for target in _target_functions(row, functions_by_entry)
    }


def _resolved_edge_count(
    rows: list[dict[str, Any]],
    functions_by_entry: dict[str, dict[str, Any]],
) -> int:
    return sum(
        1
        for row in rows
        for entry in set(_values(row.get("call_target_entries")))
        if entry in functions_by_entry
    )


def compare(
    campaign_rows: list[dict[str, Any]],
    multiplayer_rows: list[dict[str, Any]],
    keywords: tuple[str, ...] = DEFAULT_KEYWORDS,
) -> dict[str, Any]:
    campaign = _group(campaign_rows)
    multiplayer = _group(multiplayer_rows)
    keyword_values = tuple(value.lower() for value in keywords if value)

    campaign_by_entry = _entry_map(campaign_rows)
    multiplayer_by_entry = _entry_map(multiplayer_rows)
    campaign_fingerprints = set(campaign)
    multiplayer_fingerprints = set(multiplayer)

    matched = []
    multiplayer_only = []
    campaign_only = []
    call_graph_deltas = []
    for fingerprint in sorted(set(campaign) | set(multiplayer)):
        c_rows = campaign.get(fingerprint, [])
        m_rows = multiplayer.get(fingerprint, [])
        if c_rows and m_rows:
            matched.append({
                "fingerprint": fingerprint,
                "campaign_count": len(c_rows),
                "multiplayer_count": len(m_rows),
                "campaign_functions": c_rows,
                "multiplayer_functions": m_rows,
                "multiplicity_delta": len(m_rows) - len(c_rows),
            })
            campaign_targets = set().union(*(
                _target_fingerprints(row, campaign_by_entry) for row in c_rows
            ))
            multiplayer_targets = set().union(*(
                _target_fingerprints(row, multiplayer_by_entry) for row in m_rows
            ))
            if campaign_targets != multiplayer_targets:
                campaign_callees = {
                    target["entry"]: target
                    for row in c_rows
                    for target in _target_functions(row, campaign_by_entry)
                }
                multiplayer_callees = {
                    target["entry"]: target
                    for row in m_rows
                    for target in _target_functions(row, multiplayer_by_entry)
                }
                call_graph_deltas.append({
                    "caller_fingerprint": fingerprint,
                    "campaign_functions": [
                        {"name": row["name"], "entry": row["entry"]} for row in c_rows
                    ],
                    "multiplayer_functions": [
                        {"name": row["name"], "entry": row["entry"]} for row in m_rows
                    ],
                    "campaign_callee_fingerprints": sorted(campaign_targets),
                    "multiplayer_callee_fingerprints": sorted(multiplayer_targets),
                    "campaign_callee_functions": [
                        {
                            "name": target["name"],
                            "entry": target["entry"],
                            "fingerprint": target["fingerprint"],
                        }
                        for target in sorted(
                            campaign_callees.values(),
                            key=lambda item: (item["fingerprint"], item["entry"]),
                        )
                    ],
                    "multiplayer_callee_functions": [
                        {
                            "name": target["name"],
                            "entry": target["entry"],
                            "fingerprint": target["fingerprint"],
                        }
                        for target in sorted(
                            multiplayer_callees.values(),
                            key=lambda item: (item["fingerprint"], item["entry"]),
                        )
                    ],
                    "multiplayer_only_callee_fingerprints": sorted(
                        multiplayer_targets - campaign_targets
                    ),
                    "campaign_only_callee_fingerprints": sorted(
                        campaign_targets - multiplayer_targets
                    ),
                })
        elif m_rows:
            multiplayer_only.append({
                "fingerprint": fingerprint,
                "functions": m_rows,
            })
        else:
            campaign_only.append({
                "fingerprint": fingerprint,
                "functions": c_rows,
            })

    incoming_by_entry: dict[str, dict[str, dict[str, Any]]] = defaultdict(dict)
    for caller in multiplayer_rows:
        for target_entry in set(_values(caller.get("call_target_entries"))):
            if target_entry in multiplayer_by_entry:
                incoming_by_entry[target_entry][caller["entry"]] = caller

    priority = []
    for group in multiplayer_only:
        for function in group["functions"]:
            name_hits = _keyword_hits(function["name"], keyword_values)
            target_symbols = _values(function.get("call_target_symbols"))
            target_hits = [
                keyword for keyword in keyword_values
                if any(keyword in symbol.lower() for symbol in target_symbols)
            ]

            incoming_callers = []
            incoming_name_hits: set[str] = set()
            incoming_target_hits: set[str] = set()
            for caller in sorted(
                incoming_by_entry.get(function["entry"], {}).values(),
                key=lambda item: (item["entry"], item["name"]),
            ):
                caller_name_hits = _keyword_hits(caller["name"], keyword_values)
                caller_target_hits = _keyword_hits(
                    " ".join(_values(caller.get("call_target_symbols"))),
                    keyword_values,
                )
                incoming_name_hits.update(caller_name_hits)
                incoming_target_hits.update(caller_target_hits)
                incoming_callers.append({
                    "name": caller["name"],
                    "entry": caller["entry"],
                    "fingerprint": caller["fingerprint"],
                    "name_keyword_hits": caller_name_hits,
                    "context_call_target_keyword_hits": caller_target_hits,
                })

            outgoing_target_functions = _target_functions(function, multiplayer_by_entry)
            outgoing_targets = {
                target["fingerprint"] for target in outgoing_target_functions
            }
            outgoing_shared = outgoing_targets & campaign_fingerprints
            outgoing_multiplayer_only = outgoing_targets - campaign_fingerprints
            incoming_name_hit_list = sorted(incoming_name_hits)
            incoming_target_hit_list = sorted(incoming_target_hits)
            evidence_score = (
                len(name_hits)
                + len(target_hits)
                + len(incoming_name_hit_list)
                + len(incoming_target_hit_list)
            )
            if evidence_score:
                priority.append({
                    "fingerprint": group["fingerprint"],
                    "function": function,
                    "keyword_hits": name_hits,
                    "call_target_keyword_hits": target_hits,
                    "incoming_caller_keyword_hits": incoming_name_hit_list,
                    "incoming_caller_context_target_keyword_hits": incoming_target_hit_list,
                    "incoming_callers": incoming_callers,
                    "outgoing_target_fingerprints": sorted(outgoing_targets),
                    "outgoing_target_functions": [
                        {
                            "name": target["name"],
                            "entry": target["entry"],
                            "fingerprint": target["fingerprint"],
                            "fingerprint_present_in_campaign": (
                                target["fingerprint"] in campaign_fingerprints
                            ),
                        }
                        for target in outgoing_target_functions
                    ],
                    "outgoing_shared_target_fingerprints": sorted(outgoing_shared),
                    "outgoing_multiplayer_only_target_fingerprints": sorted(
                        outgoing_multiplayer_only
                    ),
                    "evidence_score": evidence_score,
                    "classification": "MULTIPLAYER_ONLY_CALL_GRAPH_OR_SYMBOL_LEAD",
                })

    priority.sort(
        key=lambda item: (
            -item["evidence_score"],
            -len(item["incoming_caller_keyword_hits"]),
            -len(item["keyword_hits"]),
            item["function"]["name"].lower(),
            item["function"]["entry"],
        )
    )

    campaign_edges = _resolved_edge_count(campaign_rows, campaign_by_entry)
    multiplayer_edges = _resolved_edge_count(multiplayer_rows, multiplayer_by_entry)
    return {
        "summary": {
            "campaign_function_records": len(campaign_rows),
            "multiplayer_function_records": len(multiplayer_rows),
            "campaign_unique_fingerprints": len(campaign),
            "multiplayer_unique_fingerprints": len(multiplayer),
            "matching_fingerprint_groups": len(matched),
            "multiplayer_only_fingerprint_groups": len(multiplayer_only),
            "campaign_only_fingerprint_groups": len(campaign_only),
            "priority_multiplayer_only_name_matches": sum(
                1 for item in priority if item["keyword_hits"]
            ),
            "priority_multiplayer_only_call_target_matches": sum(
                1 for item in priority if item["call_target_keyword_hits"]
            ),
            "priority_multiplayer_only_incoming_caller_matches": sum(
                1 for item in priority
                if item["incoming_caller_keyword_hits"]
                or item["incoming_caller_context_target_keyword_hits"]
            ),
            "priority_multiplayer_only_leads": len(priority),
            "campaign_resolved_internal_call_edges": campaign_edges,
            "multiplayer_resolved_internal_call_edges": multiplayer_edges,
            "matching_fingerprint_groups_with_call_graph_delta": len(call_graph_deltas),
        },
        "matched_fingerprints": matched,
        "call_graph_deltas": call_graph_deltas,
        "multiplayer_only": multiplayer_only,
        "campaign_only": campaign_only,
        "priority_multiplayer_only": priority,
        "call_graph_analysis": {
            "campaign_function_entries": len(campaign_by_entry),
            "multiplayer_function_entries": len(multiplayer_by_entry),
            "campaign_resolved_internal_call_edges": campaign_edges,
            "multiplayer_resolved_internal_call_edges": multiplayer_edges,
            "matching_fingerprint_groups_with_call_graph_delta": len(call_graph_deltas),
            "limitations": [
                "Only call references resolved by Ghidra to a function body are represented as internal edges.",
                "Indirect calls, tail calls, unresolved references and missed function boundaries may be absent.",
                "A call-graph edge is static evidence, not proof that a function executes in a particular game mode.",
                "Fingerprint-level call-graph differences are navigation leads and do not establish semantic equivalence.",
            ],
        },
        "keywords": list(keyword_values),
        "interpretation_limits": [
            "The two CSV files must come from the exact matching BLUS31270 TU 1.16 Campaign and Multiplayer decrypted PPU images.",
            "Function boundaries depend on Ghidra auto-analysis and OPD/function seeding quality.",
            "The fingerprint normalizes long absolute addresses and auto-generated labels; collisions and missed matches are possible.",
            "A matching fingerprint is a navigation lead, not proof that a function has identical runtime context or is safe to transplant.",
            "A Multiplayer-only function name, resolved call-target symbol or incoming call-graph match is a search priority, not a local-client creation proof.",
            "Do not copy a function body or call target into Campaign until TOC, imports, global state, calling convention and runtime lifecycle are verified.",
            "This tool never identifies or approves hook addresses.",
        ],
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--campaign", required=True, type=Path)
    parser.add_argument("--multiplayer", required=True, type=Path)
    parser.add_argument("--output", type=Path, default=Path("ghosts_ppu_function_comparison.json"))
    parser.add_argument(
        "--keywords",
        default=",".join(DEFAULT_KEYWORDS),
        help="comma-separated function-name keywords used to rank Multiplayer-only leads",
    )
    args = parser.parse_args(argv)
    try:
        campaign = read_inventory(args.campaign)
        multiplayer = read_inventory(args.multiplayer)
        result = compare(
            campaign,
            multiplayer,
            tuple(value.strip() for value in args.keywords.split(",") if value.strip()),
        )
    except (OSError, InventoryError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(json.dumps(result["summary"], indent=2))
    print(f"Report: {args.output}")
    print("Candidate matches are research leads only; no hook address is approved.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
