#!/usr/bin/env python3
"""Compare Ghidra function inventories and their resolved internal call graphs.

The inventories are exported separately by ExportGhostsPpuFunctionInventory.java.
This tool ranks cross-build code-fingerprint matches and Multiplayer-only
function leads. It never emits a hook address or claims semantic equivalence.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
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


OPD_OPTIONAL_COLUMNS = {
    "opd_descriptor_addresses",
    "opd_toc_mappings",
    "opd_descriptor_count",
}
OPD_RESOLUTION_COLUMNS = {
    "opd_exact_entry_descriptor_count",
    "opd_containing_function_descriptor_count",
}
OPD_REGION_COLUMNS = {"opd_toc_region_evidence"}
OPD_TOC_MAPPING_RE = re.compile(r"^0x[0-9a-fA-F]{8}:(?:MAPPED|UNMAPPED|ZERO)$")


def _opd_toc_mappings(value: Any, path: Path, line_number: int) -> list[str]:
    mappings = _values(value)
    if len(set(mappings)) != len(mappings):
        raise InventoryError(f"{path}:{line_number}: duplicate OPD TOC mapping")
    for mapping in mappings:
        if not OPD_TOC_MAPPING_RE.fullmatch(mapping):
            raise InventoryError(f"{path}:{line_number}: malformed OPD TOC mapping {mapping!r}")
        address, status = mapping.split(":", 1)
        is_zero = int(address, 16) == 0
        if (status == "ZERO") != is_zero:
            raise InventoryError(f"{path}:{line_number}: OPD TOC mapping status contradicts its address")
    return mappings


def _opd_toc_regions(
    value: Any,
    mappings: list[str],
    path: Path,
    line_number: int,
) -> list[dict[str, str]]:
    try:
        regions = json.loads(value or "[]")
    except (TypeError, json.JSONDecodeError) as exc:
        raise InventoryError(f"{path}:{line_number}: OPD TOC region evidence is not valid JSON") from exc
    if not isinstance(regions, list):
        raise InventoryError(f"{path}:{line_number}: OPD TOC region evidence must be a JSON array")
    expected = set(mappings)
    actual: set[str] = set()
    parsed: list[dict[str, str]] = []
    for region in regions:
        if not isinstance(region, dict):
            raise InventoryError(f"{path}:{line_number}: OPD TOC region record must be an object")
        fields = ("toc", "status", "block_name", "permissions", "block_offset")
        if any(not isinstance(region.get(field), str) for field in fields):
            raise InventoryError(f"{path}:{line_number}: OPD TOC region fields must be strings")
        provenance = {
            key: region.get(key)
            for key in ("loaded", "initialized", "artificial", "external", "volatile")
        }
        address = region["toc"]
        status = region["status"]
        block_name = region["block_name"]
        permissions = region["permissions"]
        block_offset = region["block_offset"]
        if not re.fullmatch(r"0x[0-9a-fA-F]{8}", address):
            raise InventoryError(f"{path}:{line_number}: malformed OPD TOC region address")
        if status not in {"MAPPED", "UNMAPPED", "ZERO"}:
            raise InventoryError(f"{path}:{line_number}: unsupported OPD TOC region status")
        is_zero = int(address, 16) == 0
        if (status == "ZERO") != is_zero:
            raise InventoryError(f"{path}:{line_number}: OPD TOC region status contradicts its address")
        if status == "MAPPED":
            if (
                not block_name
                or not re.fullmatch(r"[r-][w-][x-]", permissions)
                or not re.fullmatch(r"0x[0-9a-fA-F]{8}", block_offset)
                or any(not isinstance(flag, bool) for flag in provenance.values())
            ):
                raise InventoryError(f"{path}:{line_number}: mapped OPD TOC region lacks block permissions, offset or provenance flags")
        elif block_name or permissions or block_offset or any(
            flag is not None for flag in provenance.values()
        ):
            raise InventoryError(f"{path}:{line_number}: unmapped/zero OPD TOC region cannot claim a memory block")
        key = f"{address}:{status}"
        if key in actual:
            raise InventoryError(f"{path}:{line_number}: duplicate OPD TOC region")
        actual.add(key)
        parsed.append({field: region[field] for field in fields})
    if actual != expected:
        raise InventoryError(f"{path}:{line_number}: OPD TOC regions do not match TOC mapping statuses")
    return sorted(parsed, key=lambda item: item["toc"])


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

        present_opd_columns = OPD_OPTIONAL_COLUMNS & columns
        if present_opd_columns and present_opd_columns != OPD_OPTIONAL_COLUMNS:
            missing_opd = OPD_OPTIONAL_COLUMNS - present_opd_columns
            raise InventoryError(
                f"{path}: incomplete OPD evidence columns: {', '.join(sorted(missing_opd))}"
            )
        present_region_columns = OPD_REGION_COLUMNS & columns
        if present_region_columns and present_opd_columns != OPD_OPTIONAL_COLUMNS:
            raise InventoryError(f"{path}: OPD TOC region evidence requires the complete OPD evidence columns")
        present_resolution_columns = OPD_RESOLUTION_COLUMNS & columns
        if present_resolution_columns and present_resolution_columns != OPD_RESOLUTION_COLUMNS:
            missing_resolution = OPD_RESOLUTION_COLUMNS - present_resolution_columns
            raise InventoryError(
                f"{path}: incomplete OPD resolution columns: {', '.join(sorted(missing_resolution))}"
            )
        if present_resolution_columns and present_opd_columns != OPD_OPTIONAL_COLUMNS:
            raise InventoryError(f"{path}: OPD resolution columns require the complete OPD evidence columns")
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
            opd_descriptor_addresses = _values(row.get("opd_descriptor_addresses"))
            opd_toc_mappings = _opd_toc_mappings(row.get("opd_toc_mappings"), path, line_number)
            try:
                opd_descriptor_count = (
                    int(row.get("opd_descriptor_count") or "0")
                    if present_opd_columns else 0
                )
            except (TypeError, ValueError) as exc:
                raise InventoryError(
                    f"{path}:{line_number}: OPD descriptor count must be a non-negative integer"
                ) from exc
            if opd_descriptor_count < 0:
                raise InventoryError(
                    f"{path}:{line_number}: OPD descriptor count must be a non-negative integer"
                )
            if len(set(opd_descriptor_addresses)) != len(opd_descriptor_addresses):
                raise InventoryError(f"{path}:{line_number}: duplicate OPD descriptor address")
            if opd_descriptor_count != len(opd_descriptor_addresses):
                raise InventoryError(
                    f"{path}:{line_number}: OPD descriptor count does not match descriptor addresses"
                )
            if (opd_descriptor_count == 0) != (not opd_toc_mappings):
                raise InventoryError(
                    f"{path}:{line_number}: OPD descriptor count and TOC mappings do not reconcile"
                )
            opd_toc_region_evidence = (
                _opd_toc_regions(
                    row.get("opd_toc_region_evidence"),
                    opd_toc_mappings,
                    path,
                    line_number,
                )
                if present_region_columns else None
            )
            opd_exact_entry_descriptor_count = None
            opd_containing_function_descriptor_count = None
            if present_resolution_columns:
                try:
                    opd_exact_entry_descriptor_count = int(
                        row.get("opd_exact_entry_descriptor_count") or "0"
                    )
                    opd_containing_function_descriptor_count = int(
                        row.get("opd_containing_function_descriptor_count") or "0"
                    )
                except (TypeError, ValueError) as exc:
                    raise InventoryError(
                        f"{path}:{line_number}: OPD resolution counts must be non-negative integers"
                    ) from exc
                if (
                    opd_exact_entry_descriptor_count < 0
                    or opd_containing_function_descriptor_count < 0
                ):
                    raise InventoryError(
                        f"{path}:{line_number}: OPD resolution counts must be non-negative integers"
                    )
                if (
                    opd_exact_entry_descriptor_count
                    + opd_containing_function_descriptor_count
                    != opd_descriptor_count
                ):
                    raise InventoryError(
                        f"{path}:{line_number}: OPD exact/containing resolution counts do not match descriptor count"
                    )
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
                "opd_descriptor_addresses": opd_descriptor_addresses,
                "opd_toc_mappings": opd_toc_mappings,
                "opd_descriptor_count": opd_descriptor_count,
                "opd_exact_entry_descriptor_count": opd_exact_entry_descriptor_count,
                "opd_containing_function_descriptor_count": opd_containing_function_descriptor_count,
                "opd_toc_region_evidence": opd_toc_region_evidence,
            })
    if not rows:
        raise InventoryError(f"{path}: inventory contains no function rows")
    return rows


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def validate_inventory_manifest(
    manifest_path: Path,
    campaign_path: Path,
    multiplayer_path: Path,
    campaign_rows: list[dict[str, Any]],
    multiplayer_rows: list[dict[str, Any]],
) -> dict[str, Any]:
    manifest_path = Path(manifest_path)
    campaign_path = Path(campaign_path)
    multiplayer_path = Path(multiplayer_path)
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise InventoryError(f"{manifest_path}: invalid JSON manifest") from exc

    if not isinstance(manifest, dict) or manifest.get("schema_version") != 1:
        raise InventoryError(f"{manifest_path}: unsupported inventory manifest schema")
    profile = manifest.get("operator_declared_profile")
    expected_profile = {
        "title_id": "BLUS31270",
        "title_update": "1.16",
        "analysis_language": "PowerPC:BE:64:default",
    }
    if not isinstance(profile, dict):
        raise InventoryError(f"{manifest_path}: operator-declared profile is missing")
    for key, expected in expected_profile.items():
        if profile.get(key) != expected:
            raise InventoryError(
                f"{manifest_path}: profile {key} must be {expected}, got {profile.get(key)!r}"
            )

    verified_modes = {}
    for mode, inventory_path, rows in (
        ("campaign", campaign_path, campaign_rows),
        ("multiplayer", multiplayer_path, multiplayer_rows),
    ):
        details = manifest.get(mode)
        if not isinstance(details, dict):
            raise InventoryError(f"{manifest_path}: {mode} manifest section is missing")
        if details.get("inventory_file") != inventory_path.name:
            raise InventoryError(
                f"{manifest_path}: {mode} inventory filename does not match {inventory_path.name}"
            )
        expected_inventory_hash = str(details.get("inventory_sha256") or "").lower()
        if not FINGERPRINT_RE.fullmatch(expected_inventory_hash):
            raise InventoryError(
                f"{manifest_path}: {mode} inventory SHA-256 is missing or malformed"
            )
        actual_inventory_hash = _sha256_file(inventory_path)
        if actual_inventory_hash != expected_inventory_hash:
            raise InventoryError(
                f"{manifest_path}: {mode} inventory SHA-256 does not match the manifest"
            )
        expected_elf_hash = str(details.get("elf_sha256") or "").lower()
        if not FINGERPRINT_RE.fullmatch(expected_elf_hash):
            raise InventoryError(
                f"{manifest_path}: {mode} source ELF SHA-256 is missing or malformed"
            )
        try:
            declared_count = int(details.get("function_records"))
        except (TypeError, ValueError) as exc:
            raise InventoryError(
                f"{manifest_path}: {mode} function record count is invalid"
            ) from exc
        if declared_count != len(rows):
            raise InventoryError(
                f"{manifest_path}: {mode} function record count does not match the CSV"
            )
        verified_modes[mode] = {
            "elf_sha256": expected_elf_hash,
            "inventory_sha256": actual_inventory_hash,
            "function_records": declared_count,
        }

    return {
        "status": "VALIDATED_HASHES_AND_OPERATOR_DECLARED_PROFILE",
        "manifest_path": str(manifest_path.resolve()),
        "manifest_sha256": _sha256_file(manifest_path),
        "profile": expected_profile,
        "campaign": verified_modes["campaign"],
        "multiplayer": verified_modes["multiplayer"],
        "warning": (
            "The profile is operator-declared. SHA-256 verifies the exact input and CSV "
            "files against this manifest; it does not independently prove the game update's authenticity."
        ),
    }


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


def _opd_toc_evidence(row: dict[str, Any]) -> dict[str, Any]:
    mappings = _values(row.get("opd_toc_mappings"))
    statuses = {mapping.rsplit(":", 1)[-1] for mapping in mappings}
    count = int(row.get("opd_descriptor_count", 0))
    if count == 0:
        status = "NO_OPD_DESCRIPTOR_MAPPING"
    elif "UNMAPPED" in statuses:
        status = "UNMAPPED_TOC_PRESENT"
    elif "ZERO" in statuses:
        status = "ZERO_TOC_PRESENT"
    else:
        status = "MAPPED_TOC_STATIC_ONLY"
    exact_count = row.get("opd_exact_entry_descriptor_count")
    containing_count = row.get("opd_containing_function_descriptor_count")
    if exact_count is None or containing_count is None:
        resolution_status = "NOT_RECORDED"
    elif exact_count > 0 and containing_count > 0:
        resolution_status = "MIXED_EXACT_AND_CONTAINING"
    elif exact_count > 0:
        resolution_status = "EXACT_ENTRY_MATCH"
    elif containing_count > 0:
        resolution_status = "CONTAINING_FUNCTION_ONLY"
    else:
        resolution_status = "NO_FUNCTION_MATCH"
    return {
        "status": status,
        "descriptor_count": count,
        "descriptor_addresses": _values(row.get("opd_descriptor_addresses")),
        "toc_mappings": mappings,
        "toc_region_evidence": row.get("opd_toc_region_evidence"),
        "code_pointer_resolution": {
            "status": resolution_status,
            "exact_entry_descriptor_count": exact_count,
            "containing_function_descriptor_count": containing_count,
            "interpretation": (
                "EXACT_ENTRY_MATCH means the OPD code pointer equals a Ghidra function entry. "
                "CONTAINING_FUNCTION_ONLY means the pointer lies inside a function body but is not its entry. "
                "Neither classification proves runtime safety."
            ),
        },
        "interpretation": (
            "MAPPED means the TOC address is present in Ghidra memory only; it does not prove "
            "that the TOC is correct for a native call or safe to reuse in Campaign."
        ),
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
                    "opd_toc_evidence": _opd_toc_evidence(function),
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
            "campaign_functions_with_opd_descriptors": sum(
                1 for row in campaign_rows if row.get("opd_descriptor_count", 0) > 0
            ),
            "multiplayer_functions_with_opd_descriptors": sum(
                1 for row in multiplayer_rows if row.get("opd_descriptor_count", 0) > 0
            ),
            "priority_multiplayer_only_candidates_with_opd_descriptors": sum(
                1 for item in priority if item["function"].get("opd_descriptor_count", 0) > 0
            ),
            "priority_multiplayer_only_candidates_with_unmapped_toc": sum(
                1 for item in priority
                if item["opd_toc_evidence"]["status"] == "UNMAPPED_TOC_PRESENT"
            ),
            "priority_multiplayer_only_candidates_with_exact_opd_entry": sum(
                1 for item in priority
                if item["opd_toc_evidence"]["code_pointer_resolution"]["status"]
                in {"EXACT_ENTRY_MATCH", "MIXED_EXACT_AND_CONTAINING"}
            ),
            "priority_multiplayer_only_candidates_with_containing_only_opd_match": sum(
                1 for item in priority
                if item["opd_toc_evidence"]["code_pointer_resolution"]["status"]
                == "CONTAINING_FUNCTION_ONLY"
            ),
            "priority_multiplayer_only_candidates_with_mapped_toc_block": sum(
                1 for item in priority
                if any(
                    region["status"] == "MAPPED"
                    for region in (item["opd_toc_evidence"].get("toc_region_evidence") or [])
                )
            ),
            "priority_multiplayer_only_candidates_with_writable_toc_block": sum(
                1 for item in priority
                if any(
                    region["status"] == "MAPPED" and "w" in region["permissions"]
                    for region in (item["opd_toc_evidence"].get("toc_region_evidence") or [])
                )
            ),
            "priority_multiplayer_only_candidates_with_loaded_toc_block": sum(
                1 for item in priority
                if any(
                    region["status"] == "MAPPED" and region["loaded"] is True
                    for region in (item["opd_toc_evidence"].get("toc_region_evidence") or [])
                )
            ),
            "priority_multiplayer_only_candidates_with_initialized_toc_block": sum(
                1 for item in priority
                if any(
                    region["status"] == "MAPPED" and region["initialized"] is True
                    for region in (item["opd_toc_evidence"].get("toc_region_evidence") or [])
                )
            ),
            "priority_multiplayer_only_candidates_with_artificial_toc_block": sum(
                1 for item in priority
                if any(
                    region["status"] == "MAPPED" and region["artificial"] is True
                    for region in (item["opd_toc_evidence"].get("toc_region_evidence") or [])
                )
            ),
            "priority_multiplayer_only_candidates_with_external_toc_block": sum(
                1 for item in priority
                if any(
                    region["status"] == "MAPPED" and region["external"] is True
                    for region in (item["opd_toc_evidence"].get("toc_region_evidence") or [])
                )
            ),
        },
        "matched_fingerprints": matched,
        "call_graph_deltas": call_graph_deltas,
        "multiplayer_only": multiplayer_only,
        "campaign_only": campaign_only,
        "priority_multiplayer_only": priority,
        "opd_evidence_analysis": {
            "campaign_functions_with_opd_descriptors": sum(
                1 for row in campaign_rows if row.get("opd_descriptor_count", 0) > 0
            ),
            "multiplayer_functions_with_opd_descriptors": sum(
                1 for row in multiplayer_rows if row.get("opd_descriptor_count", 0) > 0
            ),
            "interpretation": (
                "OPD metadata links address-taken function entries to their descriptor TOC values. "
                "A mapped TOC is memory-mapping evidence only, not proof of correct calling context."
            ),
        },
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
                "A mapped OPD TOC is only an address-mapping observation; it is not proof that a native function can be called safely from Campaign.",
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
    parser.add_argument(
        "--manifest",
        type=Path,
        help="optional SHA-256 manifest emitted by run_ghidra_ppu_inventory.py",
    )
    parser.add_argument("--output", type=Path, default=Path("ghosts_ppu_function_comparison.json"))
    parser.add_argument(
        "--keywords",
        default=",".join(DEFAULT_KEYWORDS),
        help="comma-separated function, call-target and incoming-caller keywords used to rank Multiplayer-only leads",
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
        if args.manifest:
            result["inventory_manifest"] = validate_inventory_manifest(
                args.manifest,
                args.campaign,
                args.multiplayer,
                campaign,
                multiplayer,
            )
        else:
            result["inventory_manifest"] = {
                "status": "NOT_PROVIDED",
                "warning": (
                    "No PPU inventory manifest was supplied; CSV contents were validated, "
                    "but their source ELF hashes and declared target profile were not cross-checked."
                ),
            }
    except (OSError, InventoryError, json.JSONDecodeError) as exc:
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
