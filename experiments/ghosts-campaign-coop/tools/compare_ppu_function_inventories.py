#!/usr/bin/env python3
"""Compare Ghidra function inventories from Ghosts Campaign and Multiplayer PPU ELFs.

The inventories are exported separately by ExportGhostsPpuFunctionInventory.java.
This tool only ranks cross-build code-fingerprint matches and Multiplayer-only
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
            rows.append({
                "program": (row.get("program") or "").strip(),
                "name": name,
                "entry": entry,
                "body_address_count": body_count,
                "instruction_count": instruction_count,
                "fingerprint": fingerprint,
                "call_targets": [
                    value for value in (row.get("call_targets") or "").split(";") if value
                ],
                "call_target_symbols": [
                    value for value in (row.get("call_target_symbols") or "").split(";") if value
                ],
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


def compare(
    campaign_rows: list[dict[str, Any]],
    multiplayer_rows: list[dict[str, Any]],
    keywords: tuple[str, ...] = DEFAULT_KEYWORDS,
) -> dict[str, Any]:
    campaign = _group(campaign_rows)
    multiplayer = _group(multiplayer_rows)
    keyword_values = tuple(value.lower() for value in keywords if value)

    matched = []
    multiplayer_only = []
    campaign_only = []
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

    priority = []
    for group in multiplayer_only:
        for function in group["functions"]:
            haystack = function["name"].lower()
            hits = [keyword for keyword in keyword_values if keyword in haystack]
            target_symbols = function.get("call_target_symbols", [])
            if isinstance(target_symbols, str):
                target_symbols = [
                    value for value in target_symbols.split(";") if value
                ]
            target_hits = [
                keyword for keyword in keyword_values
                if any(keyword in symbol.lower() for symbol in target_symbols)
            ]
            if hits or target_hits:
                priority.append({
                    "fingerprint": group["fingerprint"],
                    "function": function,
                    "keyword_hits": hits,
                    "call_target_keyword_hits": target_hits,
                    "classification": "MULTIPLAYER_ONLY_NAME_OR_CALL_TARGET_MATCH_LEAD",
                })

    priority.sort(
        key=lambda item: (
            -(
                len(item["keyword_hits"])
                + len(item["call_target_keyword_hits"])
            ),
            -len(item["keyword_hits"]),
            item["function"]["name"].lower(),
            item["function"]["entry"],
        )
    )
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
            "priority_multiplayer_only_leads": len(priority),
        },
        "matched_fingerprints": matched,
        "multiplayer_only": multiplayer_only,
        "campaign_only": campaign_only,
        "priority_multiplayer_only": priority,
        "keywords": list(keyword_values),
        "interpretation_limits": [
            "The two CSV files must come from the exact matching BLUS31270 TU 1.16 Campaign and Multiplayer decrypted PPU images.",
            "Function boundaries depend on Ghidra auto-analysis and OPD/function seeding quality.",
            "The fingerprint normalizes long absolute addresses and auto-generated labels; collisions and missed matches are possible.",
            "A matching fingerprint is a navigation lead, not proof that a function has identical runtime context or is safe to transplant.",
            "A Multiplayer-only function name or resolved call-target symbol match is a search priority, not a local-client creation proof.",
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
