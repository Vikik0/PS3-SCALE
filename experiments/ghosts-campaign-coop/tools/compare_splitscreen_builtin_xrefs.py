#!/usr/bin/env python3
"""Compare actual call references to split-screen builtins across Campaign/Multiplayer."""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
import sys
from collections import defaultdict
from pathlib import Path

XREF_FIELDS = {
    "program", "builtin", "string_address", "reference_address", "reference_type",
    "is_call", "is_data", "source_type", "caller_name", "caller_entry",
    "caller_body_start", "caller_body_end", "instruction",
}
INVENTORY_FIELDS = {"name", "entry", "fingerprint"}
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")


class BuiltinXrefError(ValueError):
    """Malformed Ghidra xref or PPU inventory input."""


def address_key(value, required=False):
    token = (value or "").strip()
    if not token:
        if required:
            raise BuiltinXrefError("required address is empty")
        return None
    if ":" in token:
        token = token.rsplit(":", 1)[-1]
    if token.lower().startswith("0x"):
        token = token[2:]
    if not re.fullmatch(r"[0-9a-fA-F]+", token):
        raise BuiltinXrefError(f"invalid hexadecimal address: {value!r}")
    return f"{int(token, 16):016x}"


def parse_flag(value, label, path, line):
    token = (value or "").strip().lower()
    if token == "true":
        return True
    if token == "false":
        return False
    raise BuiltinXrefError(f"{path}:{line}: {label} must be true or false")


def read_inventory(path):
    path = Path(path)
    if not path.is_file():
        raise BuiltinXrefError(f"function inventory does not exist: {path}")
    with path.open(encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        missing = sorted(INVENTORY_FIELDS - set(reader.fieldnames or []))
        if missing:
            raise BuiltinXrefError(
                f"{path}: function inventory missing columns: {', '.join(missing)}"
            )
        result = {}
        for line, row in enumerate(reader, 2):
            entry = address_key(row.get("entry", ""), True)
            name = (row.get("name") or "").strip()
            fingerprint = (row.get("fingerprint") or "").strip().lower()
            if not name:
                raise BuiltinXrefError(f"{path}:{line}: function name is empty")
            if not SHA256_RE.fullmatch(fingerprint):
                raise BuiltinXrefError(f"{path}:{line}: invalid function fingerprint")
            if entry in result:
                raise BuiltinXrefError(
                    f"{path}:{line}: duplicate function entry {row.get('entry')}"
                )
            result[entry] = {
                "name": name,
                "entry": row["entry"].strip(),
                "fingerprint": fingerprint,
            }
    if not result:
        raise BuiltinXrefError(f"{path}: function inventory contains no records")
    return result


def read_xrefs(path, inventory):
    path = Path(path)
    if not path.is_file():
        raise BuiltinXrefError(f"Ghidra xref CSV does not exist: {path}")
    with path.open(encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        missing = sorted(XREF_FIELDS - set(reader.fieldnames or []))
        if missing:
            raise BuiltinXrefError(
                f"{path}: Ghidra xref CSV missing columns: {', '.join(missing)}"
            )
        rows, seen = [], set()
        for line, raw in enumerate(reader, 2):
            builtin = (raw.get("builtin") or "").strip()
            program = (raw.get("program") or "").strip()
            string_addr = address_key(raw.get("string_address", ""), True)
            ref_addr = address_key(raw.get("reference_address", ""))
            caller_addr = address_key(raw.get("caller_entry", ""))
            is_call = parse_flag(raw.get("is_call"), "is_call", path, line)
            is_data = parse_flag(raw.get("is_data"), "is_data", path, line)
            if not builtin or not program:
                raise BuiltinXrefError(
                    f"{path}:{line}: builtin and program are required"
                )
            if is_call and is_data:
                raise BuiltinXrefError(
                    f"{path}:{line}: a Ghidra reference cannot be both call and data"
                )
            if (is_call or is_data) and ref_addr is None:
                raise BuiltinXrefError(
                    f"{path}:{line}: call/data reference requires a reference address"
                )
            key = (builtin, string_addr, ref_addr or "")
            if key in seen:
                raise BuiltinXrefError(
                    f"{path}:{line}: duplicate xref row for {builtin} at {ref_addr}"
                )
            seen.add(key)
            function = inventory.get(caller_addr or "")
            if is_call:
                reference_kind = "call_xref"
            elif is_data:
                reference_kind = "data_xref"
            elif ref_addr:
                reference_kind = "other_xref"
            else:
                reference_kind = "string_without_recorded_xref"
            rows.append({
                "program": program,
                "builtin": builtin,
                "string_address": raw["string_address"].strip(),
                "reference_address": raw.get("reference_address", "").strip(),
                "reference_type": (raw.get("reference_type") or "").strip(),
                "is_call": is_call,
                "is_data": is_data,
                "source_type": (raw.get("source_type") or "").strip(),
                "caller_name": (raw.get("caller_name") or "").strip(),
                "caller_entry": raw.get("caller_entry", "").strip(),
                "caller_entry_key": caller_addr,
                "caller_fingerprint": function["fingerprint"] if function else None,
                "caller_inventory_name": function["name"] if function else None,
                "caller_inventory_match": function is not None,
                "reference_kind": reference_kind,
            })
    return rows


def caller_summary(rows):
    grouped = {}
    for row in rows:
        if not row["is_call"]:
            continue
        fingerprint = row["caller_fingerprint"]
        if not fingerprint:
            continue
        key = (fingerprint, row["caller_entry_key"])
        item = grouped.setdefault(key, {
            "name": row["caller_inventory_name"],
            "entry": row["caller_entry"],
            "fingerprint": fingerprint,
            "xref_count": 0,
        })
        item["xref_count"] += 1
    return sorted(grouped.values(), key=lambda item: (item["name"].lower(), item["entry"]))


def _reference_counts(rows):
    return {
        "xref_rows": len(rows),
        "call_xref_rows": sum(row["is_call"] for row in rows),
        "data_xref_rows": sum(row["is_data"] for row in rows),
        "other_xref_rows": sum(not row["is_call"] and not row["is_data"] for row in rows),
        "caller_inventory_misses": sum(
            row["caller_entry_key"] is not None and not row["caller_inventory_match"]
            for row in rows
        ),
    }


def compare(campaign_xrefs, multiplayer_xrefs, campaign_inventory, multiplayer_inventory, input_paths=None):
    by_mode = []
    for rows in (campaign_xrefs, multiplayer_xrefs):
        grouped = defaultdict(list)
        for row in rows:
            grouped[row["builtin"]].append(row)
        by_mode.append(grouped)
    campaign_by_builtin, multiplayer_by_builtin = by_mode
    reports, leads = [], []

    for builtin in sorted(set(campaign_by_builtin) | set(multiplayer_by_builtin)):
        campaign_rows = campaign_by_builtin.get(builtin, [])
        multiplayer_rows = multiplayer_by_builtin.get(builtin, [])
        campaign_calls = [row for row in campaign_rows if row["is_call"]]
        multiplayer_calls = [row for row in multiplayer_rows if row["is_call"]]
        campaign_callers = caller_summary(campaign_calls)
        multiplayer_callers = caller_summary(multiplayer_calls)
        campaign_fingerprints = {item["fingerprint"] for item in campaign_callers}
        multiplayer_fingerprints = {item["fingerprint"] for item in multiplayer_callers}
        shared = sorted(campaign_fingerprints & multiplayer_fingerprints)
        multiplayer_only = sorted(multiplayer_fingerprints - campaign_fingerprints)
        campaign_stats = _reference_counts(campaign_rows)
        multiplayer_stats = _reference_counts(multiplayer_rows)

        reports.append({
            "builtin": builtin,
            "campaign": {**campaign_stats, "callers": campaign_callers},
            "multiplayer": {**multiplayer_stats, "callers": multiplayer_callers},
            "shared_caller_fingerprints": shared,
            "multiplayer_only_caller_fingerprints": multiplayer_only,
            "campaign_only_caller_fingerprints": sorted(campaign_fingerprints - multiplayer_fingerprints),
            "interpretation": (
                "Only Ghidra references explicitly typed as calls contribute to caller fingerprints. "
                "Data and other references are reported separately and are not treated as call sites."
            ),
        })

        # A data/string reference alone is never promoted to a native call-path lead.
        if multiplayer_calls and (not campaign_calls or multiplayer_only):
            leads.append({
                "builtin": builtin,
                "campaign_call_xref_rows": len(campaign_calls),
                "multiplayer_call_xref_rows": len(multiplayer_calls),
                "multiplayer_only_caller_count": len(multiplayer_only),
                "multiplayer_only_callers": [
                    item for item in multiplayer_callers
                    if item["fingerprint"] in set(multiplayer_only)
                ],
                "shared_caller_count": len(shared),
                "priority": (
                    "MP_ONLY_CALL_REFERENCE"
                    if not campaign_calls
                    else "MP_ONLY_CALLER_FINGERPRINT"
                ),
            })

    leads.sort(key=lambda item: (
        0 if item["priority"] == "MP_ONLY_CALL_REFERENCE" else 1,
        -item["multiplayer_only_caller_count"],
        -item["multiplayer_call_xref_rows"],
        item["builtin"].lower(),
    ))
    result = {
        "schema_version": 2,
        "status": "SPLITSCREEN_BUILTIN_CALL_XREF_COMPARISON_COMPLETED",
        "campaign": {
            **_reference_counts(campaign_xrefs),
            "inventory_functions": len(campaign_inventory),
            "distinct_builtins": len(campaign_by_builtin),
        },
        "multiplayer": {
            **_reference_counts(multiplayer_xrefs),
            "inventory_functions": len(multiplayer_inventory),
            "distinct_builtins": len(multiplayer_by_builtin),
        },
        "builtin_comparisons": reports,
        "multiplayer_only_leads": leads,
        "limitations": [
            "Only explicit Ghidra call references are used to form caller fingerprints and Multiplayer-only leads.",
            "Data references and non-call references may still reveal registration tables or state, but are not call-site proof.",
            "The tool does not resolve indirect calls, virtual dispatch, callback execution, or runtime-only registration.",
            "Matching instruction fingerprints are navigation evidence, not proof of identical semantics or runtime behavior.",
            "No address is approved as a hook; verify exact executable, function boundary, TOC, calling convention, globals and lifecycle.",
            "A Multiplayer-only call reference does not prove Campaign can initialize a second local client.",
        ],
    }
    if input_paths:
        result["input_sha256"] = {
            key: hashlib.sha256(Path(path).read_bytes()).hexdigest()
            for key, path in sorted(input_paths.items())
        }
    return result


def compare_paths(campaign_xrefs, multiplayer_xrefs, campaign_inventory, multiplayer_inventory):
    campaign_inventory_rows = read_inventory(campaign_inventory)
    multiplayer_inventory_rows = read_inventory(multiplayer_inventory)
    campaign_rows = read_xrefs(campaign_xrefs, campaign_inventory_rows)
    multiplayer_rows = read_xrefs(multiplayer_xrefs, multiplayer_inventory_rows)
    return compare(
        campaign_rows,
        multiplayer_rows,
        campaign_inventory_rows,
        multiplayer_inventory_rows,
        {
            "campaign_xrefs": campaign_xrefs,
            "multiplayer_xrefs": multiplayer_xrefs,
            "campaign_inventory": campaign_inventory,
            "multiplayer_inventory": multiplayer_inventory,
        },
    )


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--campaign-xrefs", required=True, type=Path)
    parser.add_argument("--multiplayer-xrefs", required=True, type=Path)
    parser.add_argument("--campaign-inventory", required=True, type=Path)
    parser.add_argument("--multiplayer-inventory", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args(argv)
    try:
        report = compare_paths(
            args.campaign_xrefs,
            args.multiplayer_xrefs,
            args.campaign_inventory,
            args.multiplayer_inventory,
        )
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(
            json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        print(json.dumps({
            "status": report["status"],
            "campaign": report["campaign"],
            "multiplayer": report["multiplayer"],
            "multiplayer_only_leads": report["multiplayer_only_leads"],
        }, ensure_ascii=False, indent=2))
        return 0
    except (OSError, BuiltinXrefError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
