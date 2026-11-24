#!/usr/bin/env python3
"""Run both TU 1.16 PPU inventories through Ghidra headless analysis."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

PROJECT_ROOT = Path(__file__).resolve().parents[1]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from tools.compare_ppu_function_inventories import InventoryError, read_inventory
from tools.compare_splitscreen_builtin_xrefs import compare_paths as compare_builtin_xrefs, read_inventory as read_builtin_inventory, read_xrefs
from tools.ps3_elf_inspect import PpuElf

PROCESSOR = "PowerPC:BE:64:default"
PPU_ADDRESS_MAX = 0xFFFFFFFF
PPU_ADDRESS_SPACE = 0x100000000
PPU_OPD_ALIGNMENT = 8
PPU_INSTRUCTION_ALIGNMENT = 4
EXPORT_SCRIPT = "ExportGhostsPpuFunctionInventory.java"
XREF_SCRIPT = "FindGhostsSplitScreenBuiltins.java"
SEED_SCRIPT = "SeedGhostsOpdFunctions.java"
OUTPUT_NAMES = (
    "campaign_ppu_inventory.csv",
    "multiplayer_ppu_inventory.csv",
    "campaign_builtin_xrefs.csv",
    "multiplayer_builtin_xrefs.csv",
    "splitscreen_builtin_xrefs.json",
    "ppu_inventory_manifest.json",
)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def parse_integer(value: str, label: str) -> int:
    try:
        if value.lower().startswith("0x"):
            return int(value[2:], 16)
        return int(value, 10)
    except ValueError as exc:
        raise ValueError(f"{label} must be a decimal or 0x-prefixed integer") from exc


def _validate_opd_pair(
    address: str | None,
    size: str | None,
    label: str,
) -> tuple[int, int] | None:
    if (address is None) != (size is None):
        raise ValueError(f"{label}: provide both OPD virtual address and size, or neither")
    if address is None:
        return None
    parsed_address = parse_integer(address, f"{label} OPD virtual address")
    parsed_size = parse_integer(size, f"{label} OPD size")
    if parsed_address < 0:
        raise ValueError(f"{label}: OPD virtual address cannot be negative")
    if parsed_address > PPU_ADDRESS_MAX:
        raise ValueError(f"{label}: OPD virtual address must fit the unsigned 32-bit PPU address space")
    if parsed_address % PPU_OPD_ALIGNMENT:
        raise ValueError(f"{label}: OPD virtual address must be {PPU_OPD_ALIGNMENT}-byte aligned")
    if parsed_size < 8 or parsed_size % 8:
        raise ValueError(f"{label}: OPD size must be at least 8 and divisible by 8")
    if parsed_size > PPU_ADDRESS_SPACE - parsed_address:
        raise ValueError(f"{label}: OPD range exceeds the unsigned 32-bit PPU address space")
    return parsed_address, parsed_size


def _validate_opd_tuple(opd: tuple[int, int] | None, label: str) -> None:
    if opd is None:
        return
    if not isinstance(opd, tuple) or len(opd) != 2:
        raise ValueError(f"{label}: OPD range must be a (virtual_address, size) pair")
    address, size = opd
    if (
        isinstance(address, bool) or isinstance(size, bool)
        or not isinstance(address, int) or not isinstance(size, int)
    ):
        raise ValueError(f"{label}: OPD address and size must be integers")
    if address < 0:
        raise ValueError(f"{label}: OPD virtual address cannot be negative")
    if address > PPU_ADDRESS_MAX:
        raise ValueError(f"{label}: OPD virtual address must fit the unsigned 32-bit PPU address space")
    if address % PPU_OPD_ALIGNMENT:
        raise ValueError(f"{label}: OPD virtual address must be {PPU_OPD_ALIGNMENT}-byte aligned")
    if size < 8 or size % 8:
        raise ValueError(f"{label}: OPD size must be at least 8 and divisible by 8")
    if size > PPU_ADDRESS_SPACE - address:
        raise ValueError(f"{label}: OPD range exceeds the unsigned 32-bit PPU address space")




def _read_opd_seed_summary(
    path: Path,
    opd: tuple[int, int],
    label: str,
) -> dict[str, Any]:
    path = Path(path)
    if not path.is_file():
        raise FileNotFoundError(f"{label}: Ghidra OPD seeder did not write its summary: {path}")
    try:
        report = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise ValueError(f"{label}: Ghidra OPD seeder summary is not valid JSON") from exc
    if not isinstance(report, dict) or report.get("schema_version") != 2:
        raise ValueError(f"{label}: unsupported Ghidra OPD seeder summary schema")
    if report.get("status") != "OPD_SEEDING_COMPLETED":
        raise ValueError(f"{label}: Ghidra OPD seeding did not complete successfully")
    expected_address, expected_size = hex(opd[0]), hex(opd[1])
    if str(report.get("opd_va", "")).lower() != expected_address.lower():
        raise ValueError(f"{label}: OPD seeder summary address does not match the requested range")
    if str(report.get("opd_size", "")).lower() != expected_size.lower():
        raise ValueError(f"{label}: OPD seeder summary size does not match the requested range")
    count_fields = (
        "descriptor_slots_scanned",
        "empty_descriptors",
        "nonempty_descriptors",
        "unreadable_descriptors",
        "valid_code_pointers",
        "invalid_code_pointers",
        "functions_created",
        "functions_already_present",
        "functions_failed_to_create",
        "distinct_toc_values",
        "mapped_toc_values",
        "unmapped_toc_values",
        "zero_toc_descriptors",
    )
    counts = {}
    for field in count_fields:
        value = report.get(field)
        if isinstance(value, bool) or not isinstance(value, int) or value < 0:
            raise ValueError(f"{label}: OPD seeder summary field {field} must be a non-negative integer")
        counts[field] = value

    expected_slots = opd[1] // 8
    if counts["descriptor_slots_scanned"] != expected_slots:
        raise ValueError(f"{label}: OPD descriptor slot count does not match the requested size")
    classified_slots = (
        counts["empty_descriptors"]
        + counts["nonempty_descriptors"]
        + counts["unreadable_descriptors"]
    )
    if classified_slots != counts["descriptor_slots_scanned"]:
        raise ValueError(f"{label}: OPD descriptor classifications do not reconcile")
    classified_code_pointers = (
        counts["valid_code_pointers"] + counts["invalid_code_pointers"]
    )
    if classified_code_pointers != counts["nonempty_descriptors"]:
        raise ValueError(f"{label}: OPD code-pointer counts do not reconcile")
    if counts["valid_code_pointers"] == 0:
        raise ValueError(f"{label}: OPD seeder found no code pointers in executable memory")
    accounted = (
        counts["functions_created"]
        + counts["functions_already_present"]
        + counts["functions_failed_to_create"]
    )
    if accounted != counts["valid_code_pointers"]:
        raise ValueError(f"{label}: OPD seeder function counts do not reconcile")
    if counts["functions_created"] + counts["functions_already_present"] == 0:
        raise ValueError(f"{label}: OPD seeder created or found no functions")
    if counts["distinct_toc_values"] != counts["mapped_toc_values"] + counts["unmapped_toc_values"]:
        raise ValueError(f"{label}: OPD TOC mapping counts do not reconcile")
    if counts["zero_toc_descriptors"] > counts["nonempty_descriptors"]:
        raise ValueError(f"{label}: OPD zero-TOC descriptor count is invalid")
    if counts["distinct_toc_values"] > (
        counts["nonempty_descriptors"] - counts["zero_toc_descriptors"]
    ):
        raise ValueError(f"{label}: OPD distinct TOC count exceeds non-zero TOC descriptors")
    return report


def _read_opd_function_resolution_summary(
    path: Path,
    opd: tuple[int, int],
    label: str,
    inventory_rows: list[dict[str, Any]],
    seed_summary: dict[str, Any],
) -> dict[str, Any]:
    path = Path(path)
    if not path.is_file():
        raise FileNotFoundError(
            f"{label}: Ghidra inventory exporter did not write its OPD resolution summary: {path}"
        )
    try:
        report = json.loads(path.read_text(encoding="utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ValueError(f"{label}: OPD function-resolution summary is not valid UTF-8 JSON") from exc
    if not isinstance(report, dict) or report.get("schema_version") != 1:
        raise ValueError(f"{label}: unsupported OPD function-resolution summary schema")
    if report.get("status") != "OPD_FUNCTION_RESOLUTION_COMPLETED":
        raise ValueError(f"{label}: OPD function-resolution analysis did not complete")
    try:
        reported_address = parse_integer(str(report.get("opd_va", "")), f"{label} resolution OPD address")
        reported_size = parse_integer(str(report.get("opd_size", "")), f"{label} resolution OPD size")
    except ValueError as exc:
        raise ValueError(f"{label}: OPD function-resolution range is malformed") from exc
    if (reported_address, reported_size) != opd:
        raise ValueError(f"{label}: OPD function-resolution range does not match the requested range")

    fields = (
        "descriptor_slots_scanned",
        "empty_descriptors",
        "nonempty_descriptors",
        "unreadable_descriptors",
        "valid_executable_code_pointers",
        "invalid_code_pointers",
        "exact_entry_matches",
        "containing_function_matches",
        "unresolved_executable_pointers",
        "functions_with_opd_descriptors",
    )
    counts: dict[str, int] = {}
    for field in fields:
        value = report.get(field)
        if isinstance(value, bool) or not isinstance(value, int) or value < 0:
            raise ValueError(
                f"{label}: OPD function-resolution field {field} must be a non-negative integer"
            )
        counts[field] = value

    expected_slots = opd[1] // 8
    if counts["descriptor_slots_scanned"] != expected_slots:
        raise ValueError(f"{label}: OPD function-resolution slot count does not match requested size")
    if (
        counts["empty_descriptors"]
        + counts["nonempty_descriptors"]
        + counts["unreadable_descriptors"]
        != counts["descriptor_slots_scanned"]
    ):
        raise ValueError(f"{label}: OPD function-resolution descriptor counts do not reconcile")
    if (
        counts["valid_executable_code_pointers"] + counts["invalid_code_pointers"]
        != counts["nonempty_descriptors"]
    ):
        raise ValueError(f"{label}: OPD function-resolution code-pointer counts do not reconcile")
    if (
        counts["exact_entry_matches"]
        + counts["containing_function_matches"]
        + counts["unresolved_executable_pointers"]
        != counts["valid_executable_code_pointers"]
    ):
        raise ValueError(f"{label}: OPD function-resolution matches do not reconcile")

    seed_pairs = {
        "descriptor_slots_scanned": "descriptor_slots_scanned",
        "empty_descriptors": "empty_descriptors",
        "nonempty_descriptors": "nonempty_descriptors",
        "unreadable_descriptors": "unreadable_descriptors",
        "valid_executable_code_pointers": "valid_code_pointers",
        "invalid_code_pointers": "invalid_code_pointers",
    }
    for resolution_field, seed_field in seed_pairs.items():
        if counts[resolution_field] != seed_summary[seed_field]:
            raise ValueError(
                f"{label}: OPD function-resolution {resolution_field} does not match seeder {seed_field}"
            )

    rows_with_descriptors = sum(
        1 for row in inventory_rows if row.get("opd_descriptor_count", 0) > 0
    )
    csv_descriptor_count = sum(
        int(row.get("opd_descriptor_count", 0)) for row in inventory_rows
    )
    if any(
        row.get("opd_exact_entry_descriptor_count") is None
        or row.get("opd_containing_function_descriptor_count") is None
        for row in inventory_rows
    ):
        raise ValueError(f"{label}: OPD inventory CSV lacks exact/containing resolution columns")
    csv_exact_count = sum(
        int(row.get("opd_exact_entry_descriptor_count") or 0) for row in inventory_rows
    )
    csv_containing_count = sum(
        int(row.get("opd_containing_function_descriptor_count") or 0)
        for row in inventory_rows
    )
    if counts["functions_with_opd_descriptors"] != rows_with_descriptors:
        raise ValueError(f"{label}: OPD function count does not match inventory CSV")
    if counts["exact_entry_matches"] != csv_exact_count:
        raise ValueError(f"{label}: exact-entry count does not match per-function inventory")
    if counts["containing_function_matches"] != csv_containing_count:
        raise ValueError(f"{label}: containing-function count does not match per-function inventory")
    if counts["exact_entry_matches"] + counts["containing_function_matches"] != csv_descriptor_count:
        raise ValueError(f"{label}: mapped descriptor count does not match per-function inventory")

    return report


def _read_elf_inspector_opd_report(
    report_path: Path,
    elf_path: Path,
    label: str,
) -> tuple[tuple[int, int], dict[str, Any]]:
    report_path = Path(report_path).expanduser().resolve()
    elf_path = Path(elf_path).expanduser().resolve()
    if not report_path.is_file():
        raise FileNotFoundError(f"{label}: ELF inspector report was not found: {report_path}")
    raw = report_path.read_bytes()
    report_sha256 = hashlib.sha256(raw).hexdigest()
    elf_bytes = elf_path.read_bytes()
    actual_elf_hash = hashlib.sha256(elf_bytes).hexdigest()
    try:
        parsed_elf = PpuElf(elf_bytes)
        report = json.loads(raw.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ValueError(f"{label}: ELF inspector report is not valid UTF-8 JSON") from exc
    if not isinstance(report, dict) or report.get("tool") != "ps3_elf_inspect":
        raise ValueError(f"{label}: unsupported ELF inspector report")
    if report.get("tool_version") != "0.1.2":
        raise ValueError(f"{label}: ELF inspector report must use tool version 0.1.2")
    input_info = report.get("input")
    if not isinstance(input_info, dict):
        raise ValueError(f"{label}: ELF inspector input provenance is missing")
    report_elf_hash = input_info.get("sha256")
    if (
        not isinstance(report_elf_hash, str)
        or len(report_elf_hash) != 64
        or any(character not in "0123456789abcdef" for character in report_elf_hash.lower())
    ):
        raise ValueError(f"{label}: ELF inspector input SHA-256 is missing or malformed")
    if report_elf_hash.lower() != actual_elf_hash:
        raise ValueError(f"{label}: ELF inspector report SHA-256 does not match the supplied ELF")

    profile = report.get("operator_asserted_profile")
    expected_profile = {
        "title_id": "BLUS31270",
        "title_update": "1.16",
        "firmware_family": "CEX",
        "firmware_version": "4.93",
    }
    if not isinstance(profile, dict) or any(
        profile.get(key) != expected for key, expected in expected_profile.items()
    ):
        raise ValueError(f"{label}: ELF inspector report profile does not match BLUS31270 TU 1.16 CEX 4.93")

    elf_info = report.get("elf")
    if not isinstance(elf_info, dict):
        raise ValueError(f"{label}: ELF inspector ELF metadata is missing")
    if (
        elf_info.get("machine") != parsed_elf.header["machine"]
        or elf_info.get("machine") != 21
        or elf_info.get("endianness") != "big"
    ):
        raise ValueError(f"{label}: ELF inspector report is not a big-endian PPC64 ELF")
    if elf_info.get("type_name") not in {"ET_EXEC", "ET_DYN"}:
        raise ValueError(f"{label}: ELF inspector report has an unsupported ELF type")

    opd_info = elf_info.get("opd")
    if not isinstance(opd_info, dict):
        raise ValueError(f"{label}: ELF inspector OPD evidence is missing")
    candidate = opd_info.get("operator_seed_range_candidate")
    if not isinstance(candidate, dict):
        raise ValueError(f"{label}: ELF inspector did not provide a structurally valid OPD range candidate")
    if opd_info.get("section_type") == 8:
        raise ValueError(f"{label}: OPD section cannot be SHT_NOBITS")
    for flag in (
        "section_payload_readable",
        "section_address_aligned",
        "section_size_aligned",
        "section_alignment_valid",
        "section_address_matches_declared_alignment",
    ):
        if opd_info.get(flag) is not True:
            raise ValueError(f"{label}: ELF inspector OPD section failed {flag}")

    address = candidate.get("virtual_address")
    size = candidate.get("size")
    if not isinstance(address, str) or not isinstance(size, str):
        raise ValueError(f"{label}: OPD range candidate address and size must be strings")
    opd = _validate_opd_pair(address, size, label)
    if opd is None:
        raise ValueError(f"{label}: OPD range candidate is empty")
    if str(opd_info.get("section_address", "")).lower() != address.lower():
        raise ValueError(f"{label}: OPD candidate address does not match its section")
    if str(opd_info.get("section_size", "")).lower() != size.lower():
        raise ValueError(f"{label}: OPD candidate size does not match its section")
    candidate_section_index = candidate.get("section_index")
    candidate_section_name = candidate.get("section_name")
    if (
        isinstance(candidate_section_index, bool)
        or not isinstance(candidate_section_index, int)
        or candidate_section_index < 0
        or not isinstance(candidate_section_name, str)
    ):
        raise ValueError(f"{label}: OPD candidate section identity is malformed")
    if candidate_section_index != opd_info.get("section_index"):
        raise ValueError(f"{label}: OPD candidate section index does not match its section")
    if candidate_section_name != opd_info.get("section_name"):
        raise ValueError(f"{label}: OPD candidate section name does not match its section")
    section_alignment = opd_info.get("section_alignment")
    if (
        isinstance(section_alignment, bool)
        or not isinstance(section_alignment, int)
        or section_alignment < 8
        or (section_alignment & (section_alignment - 1)) != 0
        or opd[0] % section_alignment != 0
    ):
        raise ValueError(f"{label}: OPD section does not satisfy its declared alignment")

    count_fields = (
        "descriptor_slots_total",
        "descriptor_slots_scanned",
        "empty_descriptors",
        "nonempty_descriptors",
        "unreadable_descriptors",
        "valid_code_pointers",
        "invalid_code_pointers",
        "trailing_bytes",
        "zero_toc_descriptors",
        "distinct_toc_values",
        "mapped_toc_values",
        "unmapped_toc_values",
        "descriptor_count",
    )
    counts = {}
    for field in count_fields:
        value = opd_info.get(field)
        if isinstance(value, bool) or not isinstance(value, int) or value < 0:
            raise ValueError(f"{label}: ELF inspector OPD field {field} must be a non-negative integer")
        counts[field] = value
    if counts["descriptor_slots_total"] != opd[1] // 8:
        raise ValueError(f"{label}: OPD inspector slot count does not match its range")
    if counts["descriptor_slots_scanned"] != counts["descriptor_slots_total"]:
        raise ValueError(f"{label}: OPD inspector did not scan every descriptor slot")
    if counts["unreadable_descriptors"] != 0 or counts["trailing_bytes"] != 0:
        raise ValueError(f"{label}: OPD inspector reports unreadable descriptors or trailing bytes")
    if counts["empty_descriptors"] + counts["nonempty_descriptors"] != counts["descriptor_slots_total"]:
        raise ValueError(f"{label}: OPD inspector descriptor counts do not reconcile")
    if counts["valid_code_pointers"] + counts["invalid_code_pointers"] != counts["nonempty_descriptors"]:
        raise ValueError(f"{label}: OPD inspector code-pointer counts do not reconcile")
    if counts["descriptor_count"] != counts["valid_code_pointers"] or counts["valid_code_pointers"] == 0:
        raise ValueError(f"{label}: OPD inspector has no reconciled executable code pointers")
    if counts["zero_toc_descriptors"] > counts["nonempty_descriptors"]:
        raise ValueError(f"{label}: OPD inspector zero-TOC count is invalid")
    if counts["distinct_toc_values"] != counts["mapped_toc_values"] + counts["unmapped_toc_values"]:
        raise ValueError(f"{label}: OPD inspector TOC mapping counts do not reconcile")
    if counts["distinct_toc_values"] > counts["nonempty_descriptors"] - counts["zero_toc_descriptors"]:
        raise ValueError(f"{label}: OPD inspector distinct TOC count is invalid")
    if opd_info != parsed_elf.opd_info():
        raise ValueError(f"{label}: OPD inspector report does not match the supplied ELF structure")

    evidence = {
        "source": "ELF_INSPECTOR_REPORT",
        "report_path": str(report_path),
        "report_sha256": report_sha256,
        "tool_version": report["tool_version"],
        "input_elf_sha256": actual_elf_hash,
        "section_index": candidate["section_index"],
        "section_name": candidate["section_name"],
        "descriptor_slots": counts["descriptor_slots_total"],
        "valid_code_pointers": counts["valid_code_pointers"],
        "invalid_code_pointers": counts["invalid_code_pointers"],
    }
    return opd, evidence


def _run_headless(
    headless: Path,
    project_location: Path,
    project_name: str,
    elf: Path,
    output_csv: Path,
    output_xref_csv: Path,
    script_dir: Path,
    opd: tuple[int, int] | None,
    opd_summary_path: Path,
    opd_resolution_summary_path: Path,
) -> list[str]:
    args = [
        str(project_location),
        project_name,
        "-import",
        str(elf),
        "-processor",
        PROCESSOR,
        "-scriptPath",
        str(script_dir),
    ]
    if opd is not None:
        address, size = opd
        args.extend([
            "-postScript",
            SEED_SCRIPT,
            hex(address),
            hex(size),
            str(opd_summary_path),
        ])
    args.extend(["-postScript", EXPORT_SCRIPT, str(output_csv)])
    if opd is not None:
        args.extend([hex(opd[0]), hex(opd[1]), str(opd_resolution_summary_path)])
    args.extend([
        "-postScript",
        XREF_SCRIPT,
        str(output_xref_csv),
        "-deleteProject",
    ])

    if os.name == "nt" and headless.suffix.lower() in {".bat", ".cmd"}:
        command = [
            "cmd.exe",
            "/d",
            "/s",
            "/c",
            subprocess.list2cmdline([str(headless), *args]),
        ]
    else:
        command = [str(headless), *args]
    subprocess.run(command, check=True)
    return command


def _promote_outputs(
    staged: dict[str, Path],
    final: dict[str, Path],
    *,
    overwrite: bool,
) -> None:
    for name in OUTPUT_NAMES:
        if not staged[name].is_file():
            raise FileNotFoundError(f"staged Ghidra output is missing: {staged[name]}")

    output_dir = next(iter(final.values())).parent
    backup_dir = Path(tempfile.mkdtemp(prefix=".ghidra-inventory-backup-", dir=output_dir))
    moved: list[tuple[Path, Path]] = []
    promoted: list[Path] = []
    preserve_backup = False
    try:
        for name in OUTPUT_NAMES:
            target = final[name]
            if target.exists():
                if not overwrite:
                    raise FileExistsError(
                        f"output appeared during analysis: {target}; pass --overwrite to replace it"
                    )
                if not target.is_file():
                    raise IsADirectoryError(f"output path is not a file: {target}")
                backup = backup_dir / name
                os.replace(target, backup)
                moved.append((backup, target))
            os.replace(staged[name], target)
            promoted.append(target)
    except Exception as exc:
        rollback_errors: list[OSError] = []
        for target in reversed(promoted):
            try:
                if target.exists():
                    target.unlink()
            except OSError as rollback_error:
                rollback_errors.append(rollback_error)
        for backup, target in reversed(moved):
            try:
                if backup.exists():
                    os.replace(backup, target)
            except OSError as rollback_error:
                rollback_errors.append(rollback_error)
        if rollback_errors:
            preserve_backup = True
            raise RuntimeError(
                "inventory promotion failed and rollback was incomplete; "
                f"preserve recovery files at {backup_dir}"
            ) from rollback_errors[0]
        raise
    finally:
        if not preserve_backup:
            shutil.rmtree(backup_dir, ignore_errors=True)


def generate_inventories(
    analyze_headless: Path,
    campaign_elf: Path,
    multiplayer_elf: Path,
    output_dir: Path,
    *,
    script_dir: Path | None = None,
    campaign_opd: tuple[int, int] | None = None,
    multiplayer_opd: tuple[int, int] | None = None,
    campaign_opd_inspector_report: Path | None = None,
    multiplayer_opd_inspector_report: Path | None = None,
    overwrite: bool = False,
) -> dict[str, Any]:
    """Generate and validate both inventories before promoting any final output."""
    headless = Path(analyze_headless).expanduser().resolve()
    campaign_elf = Path(campaign_elf).expanduser().resolve()
    multiplayer_elf = Path(multiplayer_elf).expanduser().resolve()
    output_dir = Path(output_dir).expanduser().resolve()
    script_dir = (
        Path(script_dir).expanduser().resolve()
        if script_dir is not None
        else Path(__file__).resolve().parent / "ghidra"
    )
    if campaign_opd is not None and campaign_opd_inspector_report is not None:
        raise ValueError("Campaign: use either a manual OPD range or an ELF inspector report, not both")
    if multiplayer_opd is not None and multiplayer_opd_inspector_report is not None:
        raise ValueError("Multiplayer: use either a manual OPD range or an ELF inspector report, not both")
    _validate_opd_tuple(campaign_opd, "Campaign")
    _validate_opd_tuple(multiplayer_opd, "Multiplayer")

    if not headless.is_file():
        raise FileNotFoundError(f"Ghidra analyzeHeadless executable was not found: {headless}")
    if not campaign_elf.is_file():
        raise FileNotFoundError(f"Campaign reconstructed ELF was not found: {campaign_elf}")
    if not multiplayer_elf.is_file():
        raise FileNotFoundError(f"Multiplayer reconstructed ELF was not found: {multiplayer_elf}")
    if campaign_elf == multiplayer_elf:
        raise ValueError("Campaign and Multiplayer ELF inputs must be different files")
    campaign_elf_sha256_before = sha256_file(campaign_elf)
    multiplayer_elf_sha256_before = sha256_file(multiplayer_elf)
    campaign_opd_evidence = None
    multiplayer_opd_evidence = None
    if campaign_opd_inspector_report is not None:
        campaign_opd, campaign_opd_evidence = _read_elf_inspector_opd_report(
            campaign_opd_inspector_report, campaign_elf, "Campaign"
        )
    elif campaign_opd is not None:
        campaign_opd_evidence = {
            "source": "MANUAL_OPERATOR_RANGE",
            "virtual_address": hex(campaign_opd[0]),
            "size": hex(campaign_opd[1]),
        }
    if multiplayer_opd_inspector_report is not None:
        multiplayer_opd, multiplayer_opd_evidence = _read_elf_inspector_opd_report(
            multiplayer_opd_inspector_report, multiplayer_elf, "Multiplayer"
        )
    elif multiplayer_opd is not None:
        multiplayer_opd_evidence = {
            "source": "MANUAL_OPERATOR_RANGE",
            "virtual_address": hex(multiplayer_opd[0]),
            "size": hex(multiplayer_opd[1]),
        }
    _validate_opd_tuple(campaign_opd, "Campaign")
    _validate_opd_tuple(multiplayer_opd, "Multiplayer")
    if not script_dir.is_dir():
        raise FileNotFoundError(f"Ghidra script directory was not found: {script_dir}")
    for script_name in (EXPORT_SCRIPT, XREF_SCRIPT, SEED_SCRIPT):
        if not (script_dir / script_name).is_file():
            raise FileNotFoundError(f"required Ghidra script was not found: {script_dir / script_name}")

    output_dir.mkdir(parents=True, exist_ok=True)
    final = {name: output_dir / name for name in OUTPUT_NAMES}
    existing = [path for path in final.values() if path.exists()]
    if existing and not overwrite:
        raise FileExistsError(
            "inventory outputs already exist: "
            + ", ".join(str(path) for path in existing)
            + "; pass --overwrite to replace them"
        )

    with tempfile.TemporaryDirectory(
        prefix=".ghidra-inventory-stage-", dir=output_dir
    ) as temporary:
        stage_dir = Path(temporary)
        project_location = stage_dir / "projects"
        project_location.mkdir()
        campaign_csv = stage_dir / OUTPUT_NAMES[0]
        multiplayer_csv = stage_dir / OUTPUT_NAMES[1]
        campaign_xref_csv = stage_dir / OUTPUT_NAMES[2]
        multiplayer_xref_csv = stage_dir / OUTPUT_NAMES[3]
        comparison_json = stage_dir / OUTPUT_NAMES[4]
        campaign_opd_summary_path = stage_dir / "campaign_opd_seed_summary.json"
        multiplayer_opd_summary_path = stage_dir / "multiplayer_opd_seed_summary.json"
        campaign_opd_resolution_path = stage_dir / "campaign_opd_function_resolution.json"
        multiplayer_opd_resolution_path = stage_dir / "multiplayer_opd_function_resolution.json"

        commands = {}
        commands["campaign"] = _run_headless(
            headless,
            project_location,
            "GhostsCampaignTU116",
            campaign_elf,
            campaign_csv,
            campaign_xref_csv,
            script_dir,
            campaign_opd,
            campaign_opd_summary_path,
            campaign_opd_resolution_path,
        )
        campaign_opd_summary = (
            _read_opd_seed_summary(campaign_opd_summary_path, campaign_opd, "Campaign")
            if campaign_opd is not None else None
        )
        campaign_rows = read_inventory(campaign_csv)
        campaign_opd_resolution = (
            _read_opd_function_resolution_summary(
                campaign_opd_resolution_path, campaign_opd, "Campaign",
                campaign_rows, campaign_opd_summary,
            )
            if campaign_opd is not None else None
        )
        campaign_xref_rows = read_xrefs(
            campaign_xref_csv, read_builtin_inventory(campaign_csv)
        )

        commands["multiplayer"] = _run_headless(
            headless,
            project_location,
            "GhostsMultiplayerTU116",
            multiplayer_elf,
            multiplayer_csv,
            multiplayer_xref_csv,
            script_dir,
            multiplayer_opd,
            multiplayer_opd_summary_path,
            multiplayer_opd_resolution_path,
        )
        multiplayer_opd_summary = (
            _read_opd_seed_summary(multiplayer_opd_summary_path, multiplayer_opd, "Multiplayer")
            if multiplayer_opd is not None else None
        )
        multiplayer_rows = read_inventory(multiplayer_csv)
        multiplayer_opd_resolution = (
            _read_opd_function_resolution_summary(
                multiplayer_opd_resolution_path, multiplayer_opd, "Multiplayer",
                multiplayer_rows, multiplayer_opd_summary,
            )
            if multiplayer_opd is not None else None
        )
        multiplayer_xref_rows = read_xrefs(
            multiplayer_xref_csv, read_builtin_inventory(multiplayer_csv)
        )
        if sha256_file(campaign_elf) != campaign_elf_sha256_before:
            raise ValueError("Campaign input ELF changed during Ghidra analysis")
        if sha256_file(multiplayer_elf) != multiplayer_elf_sha256_before:
            raise ValueError("Multiplayer input ELF changed during Ghidra analysis")
        builtin_xref_comparison = compare_builtin_xrefs(
            campaign_xref_csv, multiplayer_xref_csv, campaign_csv, multiplayer_csv
        )
        comparison_json.write_text(
            json.dumps(builtin_xref_comparison, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )

        manifest = {
            "schema_version": 1,
            "created_at_utc": datetime.now(timezone.utc).isoformat(),
            "operator_declared_profile": {
                "title_id": "BLUS31270",
                "title_update": "1.16",
                "analysis_language": PROCESSOR,
                "warning": (
                    "The profile label is operator-declared. SHA-256 values identify the "
                    "exact input ELF files but do not independently prove game authenticity."
                ),
            },
            "campaign": {
                "elf_path": str(campaign_elf),
                "elf_sha256": campaign_elf_sha256_before,
                "inventory_file": OUTPUT_NAMES[0],
                "inventory_sha256": sha256_file(campaign_csv),
                "function_records": len(campaign_rows),
                "builtin_xref_file": OUTPUT_NAMES[2],
                "builtin_xref_sha256": sha256_file(campaign_xref_csv),
                "builtin_xref_rows": len(campaign_xref_rows),
                "builtin_xref_call_rows": sum(row["is_call"] for row in campaign_xref_rows),
                "builtin_xref_direct_call_rows": sum(row["is_call"] and not row["is_computed"] for row in campaign_xref_rows),
                "builtin_xref_computed_call_rows": sum(row["is_call"] and row["is_computed"] for row in campaign_xref_rows),
                "builtin_xref_data_rows": sum(row["is_data"] for row in campaign_xref_rows),
                "opd_seeding": (
                    {
                        "enabled": True,
                        "virtual_address": hex(campaign_opd[0]),
                        "size": hex(campaign_opd[1]),
                        "status": campaign_opd_summary["status"],
                        "range_evidence": campaign_opd_evidence,
                        "summary_sha256": sha256_file(campaign_opd_summary_path),
                        "summary": campaign_opd_summary,
                    }
                    if campaign_opd else {"enabled": False, "status": "NOT_REQUESTED"}
                ),
                "opd_function_resolution": (
                    {
                        "enabled": True,
                        "summary_sha256": sha256_file(campaign_opd_resolution_path),
                        "summary": campaign_opd_resolution,
                    }
                    if campaign_opd else {"enabled": False, "status": "NOT_REQUESTED"}
                ),
            },
            "builtin_xref_comparison_file": OUTPUT_NAMES[4],
            "builtin_xref_comparison_sha256": sha256_file(comparison_json),
            "multiplayer_only_builtin_lead_count": len(builtin_xref_comparison["multiplayer_only_leads"]),
            "multiplayer": {
                "elf_path": str(multiplayer_elf),
                "elf_sha256": multiplayer_elf_sha256_before,
                "inventory_file": OUTPUT_NAMES[1],
                "inventory_sha256": sha256_file(multiplayer_csv),
                "function_records": len(multiplayer_rows),
                "builtin_xref_file": OUTPUT_NAMES[3],
                "builtin_xref_sha256": sha256_file(multiplayer_xref_csv),
                "builtin_xref_rows": len(multiplayer_xref_rows),
                "builtin_xref_call_rows": sum(row["is_call"] for row in multiplayer_xref_rows),
                "builtin_xref_direct_call_rows": sum(row["is_call"] and not row["is_computed"] for row in multiplayer_xref_rows),
                "builtin_xref_computed_call_rows": sum(row["is_call"] and row["is_computed"] for row in multiplayer_xref_rows),
                "builtin_xref_data_rows": sum(row["is_data"] for row in multiplayer_xref_rows),
                "opd_seeding": (
                    {
                        "enabled": True,
                        "virtual_address": hex(multiplayer_opd[0]),
                        "size": hex(multiplayer_opd[1]),
                        "status": multiplayer_opd_summary["status"],
                        "range_evidence": multiplayer_opd_evidence,
                        "summary_sha256": sha256_file(multiplayer_opd_summary_path),
                        "summary": multiplayer_opd_summary,
                    }
                    if multiplayer_opd else {"enabled": False, "status": "NOT_REQUESTED"}
                ),
                "opd_function_resolution": (
                    {
                        "enabled": True,
                        "summary_sha256": sha256_file(multiplayer_opd_resolution_path),
                        "summary": multiplayer_opd_resolution,
                    }
                    if multiplayer_opd else {"enabled": False, "status": "NOT_REQUESTED"}
                ),
            },
            "commands": commands,
            "limitations": [
                "The operator must verify that both input ELFs came from the exact matching TU 1.16 capture pair.",
                "OPD seeding is optional. Inspector reports must hash-match the exact ELF input; manual ranges remain operator-declared and must never be guessed.",
                "Ghidra auto-analysis and function boundaries can be incomplete; compare output is static navigation evidence only.",
                "This workflow does not identify or approve a hook, create Player 2, or modify game files.",
            ],
        }
        manifest_path = stage_dir / OUTPUT_NAMES[5]
        manifest_path.write_text(
            json.dumps(manifest, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        staged = {
            OUTPUT_NAMES[0]: campaign_csv,
            OUTPUT_NAMES[1]: multiplayer_csv,
            OUTPUT_NAMES[2]: campaign_xref_csv,
            OUTPUT_NAMES[3]: multiplayer_xref_csv,
            OUTPUT_NAMES[4]: comparison_json,
            OUTPUT_NAMES[5]: manifest_path,
        }
        _promote_outputs(staged, final, overwrite=overwrite)

    return manifest


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--analyze-headless", type=Path, required=True)
    parser.add_argument("--campaign-elf", type=Path, required=True)
    parser.add_argument("--multiplayer-elf", type=Path, required=True)
    parser.add_argument("--script-dir", type=Path)
    parser.add_argument("--output-dir", type=Path, default=Path("analysis/ppu-inventory"))
    parser.add_argument("--campaign-opd-va")
    parser.add_argument("--campaign-opd-size")
    parser.add_argument("--multiplayer-opd-va")
    parser.add_argument("--multiplayer-opd-size")
    parser.add_argument("--campaign-opd-inspector-report", type=Path)
    parser.add_argument("--multiplayer-opd-inspector-report", type=Path)
    parser.add_argument("--overwrite", action="store_true")
    args = parser.parse_args(argv)

    try:
        campaign_opd = _validate_opd_pair(
            args.campaign_opd_va, args.campaign_opd_size, "Campaign"
        )
        multiplayer_opd = _validate_opd_pair(
            args.multiplayer_opd_va, args.multiplayer_opd_size, "Multiplayer"
        )
        manifest = generate_inventories(
            args.analyze_headless,
            args.campaign_elf,
            args.multiplayer_elf,
            args.output_dir,
            script_dir=args.script_dir,
            campaign_opd=campaign_opd,
            multiplayer_opd=multiplayer_opd,
            campaign_opd_inspector_report=args.campaign_opd_inspector_report,
            multiplayer_opd_inspector_report=args.multiplayer_opd_inspector_report,
            overwrite=args.overwrite,
        )
    except (OSError, ValueError, InventoryError, subprocess.CalledProcessError, RuntimeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    print("Ghidra PPU inventories generated.")
    print(f"Campaign functions: {manifest['campaign']['function_records']}")
    print(f"Multiplayer functions: {manifest['multiplayer']['function_records']}")
    print(f"Campaign builtin xref rows: {manifest['campaign']['builtin_xref_rows']} (calls: {manifest['campaign']['builtin_xref_call_rows']})")
    print(f"Multiplayer builtin xref rows: {manifest['multiplayer']['builtin_xref_rows']} (calls: {manifest['multiplayer']['builtin_xref_call_rows']})")
    print(f"Multiplayer-only builtin leads: {manifest['multiplayer_only_builtin_lead_count']}")
    print(f"Output directory: {Path(args.output_dir).resolve()}")
    print("Review ppu_inventory_manifest.json before comparing the two inventories.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
