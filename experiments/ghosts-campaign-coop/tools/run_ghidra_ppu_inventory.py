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
    if not isinstance(address, int) or not isinstance(size, int):
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
    if not isinstance(report, dict) or report.get("schema_version") != 1:
        raise ValueError(f"{label}: unsupported Ghidra OPD seeder summary schema")
    if report.get("status") != "OPD_SEEDING_COMPLETED":
        raise ValueError(f"{label}: Ghidra OPD seeding did not complete successfully")
    expected_address, expected_size = hex(opd[0]), hex(opd[1])
    if str(report.get("opd_va", "")).lower() != expected_address.lower():
        raise ValueError(f"{label}: OPD seeder summary address does not match the requested range")
    if str(report.get("opd_size", "")).lower() != expected_size.lower():
        raise ValueError(f"{label}: OPD seeder summary size does not match the requested range")
    count_fields = (
        "descriptors_visited",
        "valid_code_pointers",
        "functions_created",
        "functions_already_present",
        "functions_failed_to_create",
        "invalid_entries",
    )
    counts = {}
    for field in count_fields:
        value = report.get(field)
        if isinstance(value, bool) or not isinstance(value, int) or value < 0:
            raise ValueError(f"{label}: OPD seeder summary field {field} must be a non-negative integer")
        counts[field] = value
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
    if counts["descriptors_visited"] < counts["valid_code_pointers"]:
        raise ValueError(f"{label}: OPD seeder descriptor counts do not reconcile")
    return report

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
    args.extend([
        "-postScript",
        EXPORT_SCRIPT,
        str(output_csv),
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
        )
        campaign_opd_summary = (
            _read_opd_seed_summary(campaign_opd_summary_path, campaign_opd, "Campaign")
            if campaign_opd is not None else None
        )
        campaign_rows = read_inventory(campaign_csv)
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
        )
        multiplayer_opd_summary = (
            _read_opd_seed_summary(multiplayer_opd_summary_path, multiplayer_opd, "Multiplayer")
            if multiplayer_opd is not None else None
        )
        multiplayer_rows = read_inventory(multiplayer_csv)
        multiplayer_xref_rows = read_xrefs(
            multiplayer_xref_csv, read_builtin_inventory(multiplayer_csv)
        )
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
                "elf_sha256": sha256_file(campaign_elf),
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
                        "summary_sha256": sha256_file(campaign_opd_summary_path),
                        "summary": campaign_opd_summary,
                    }
                    if campaign_opd else {"enabled": False, "status": "NOT_REQUESTED"}
                ),
            },
            "builtin_xref_comparison_file": OUTPUT_NAMES[4],
            "builtin_xref_comparison_sha256": sha256_file(comparison_json),
            "multiplayer_only_builtin_lead_count": len(builtin_xref_comparison["multiplayer_only_leads"]),
            "multiplayer": {
                "elf_path": str(multiplayer_elf),
                "elf_sha256": sha256_file(multiplayer_elf),
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
                        "summary_sha256": sha256_file(multiplayer_opd_summary_path),
                        "summary": multiplayer_opd_summary,
                    }
                    if multiplayer_opd else {"enabled": False, "status": "NOT_REQUESTED"}
                ),
            },
            "commands": commands,
            "limitations": [
                "The operator must verify that both input ELFs came from the exact matching TU 1.16 capture pair.",
                "OPD seeding is optional and must use address/size values obtained from the ELF inspector, never guessed values.",
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
