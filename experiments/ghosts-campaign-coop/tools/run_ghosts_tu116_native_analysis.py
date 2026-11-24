#!/usr/bin/env python3
"""Run the local, read-only Ghosts TU 1.16 PPU inventory pipeline."""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
import tempfile
from pathlib import Path
from typing import Any

try:
    from .ps3_elf_inspect import inspect_file
    from .run_ghidra_ppu_inventory import OUTPUT_NAMES, generate_inventories
    from .verify_ghosts_tu116_inputs import verify_inputs
except ImportError:
    from ps3_elf_inspect import inspect_file
    from run_ghidra_ppu_inventory import OUTPUT_NAMES, generate_inventories
    from verify_ghosts_tu116_inputs import verify_inputs


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _atomic_json(path: Path, payload: dict[str, Any], overwrite: bool = False) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and not overwrite:
        raise FileExistsError(f"refusing to overwrite existing report: {path}")
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    temporary.replace(path)


def _elf_summary(report: dict[str, Any]) -> dict[str, Any]:
    elf, opd = report["elf"], report["elf"]["opd"]
    return {
        "file_name": report["input"]["file_name"],
        "size_bytes": report["input"]["size_bytes"],
        "sha256": report["input"]["sha256"],
        "entry_point": elf["entry_hex"],
        "machine": elf["machine_name"],
        "endianness": elf["endianness"],
        "program_header_count": elf["phnum"],
        "opd": {
            "section_name": opd["section_name"],
            "section_address": opd["section_address"],
            "section_size": opd["section_size"],
            "descriptor_slots_total": opd["descriptor_slots_total"],
            "descriptor_slots_scanned": opd["descriptor_slots_scanned"],
            "valid_code_pointers": opd["valid_code_pointers"],
            "invalid_code_pointers": opd["invalid_code_pointers"],
            "operator_seed_range_candidate": opd["operator_seed_range_candidate"],
        },
    }


def run_pipeline(
    disc_root: Path,
    update_root: Path,
    output_dir: Path,
    campaign_elf: Path | None = None,
    multiplayer_elf: Path | None = None,
    analyze_headless: Path | None = None,
    script_dir: Path | None = None,
    overwrite: bool = False,
) -> tuple[int, dict[str, Any]]:
    disc_root, update_root, output_dir = Path(disc_root), Path(update_root), Path(output_dir)
    preflight_path = output_dir / "native_input_preflight.json"
    status_path = output_dir / "native_analysis_status.json"
    if not overwrite:
        existing = [path for path in (preflight_path, status_path) if path.exists()]
        if existing:
            raise FileExistsError("refusing to overwrite existing native analysis reports: " + ", ".join(map(str, existing)))

    preflight = verify_inputs(disc_root, update_root, campaign_elf, multiplayer_elf)
    output_dir.mkdir(parents=True, exist_ok=True)
    _atomic_json(preflight_path, preflight, overwrite=True)
    status: dict[str, Any] = {
        "schema_version": 1,
        "tool": "run_ghosts_tu116_native_analysis",
        "preflight_report": preflight_path.name,
        "preflight_sha256": _sha256(preflight_path),
        "status": "",
        "ghidra_invoked": False,
        "generated_inventory_files": [],
        "errors": [],
    }

    if preflight["status"] != "MATCHED_RETAIL_INPUTS":
        status["status"] = "BLOCKED_RETAIL_INPUT_MISMATCH"
        status["errors"] = preflight["errors"]
        _atomic_json(status_path, status, overwrite=True)
        return 1, status

    if not preflight["readiness"]["native_ppu_comparison_ready"]:
        status["status"] = "WAITING_FOR_BOTH_DECRYPTED_PPU_ELFS"
        status["limitations"] = [
            "Retail SELF files are verified but their executable segments remain encrypted.",
            "Ghidra comparison waits for both matching decrypted TU 1.16 PPU ELFs.",
            "This local workflow does not require a PS3 connection or upload retail binaries.",
        ]
        _atomic_json(status_path, status, overwrite=True)
        return 0, status

    if analyze_headless is None:
        status["status"] = "DECRYPTED_ELFS_VERIFIED_GHIDRA_NOT_REQUESTED"
        status["limitations"] = [
            "Both decrypted PPU ELFs passed the installed TU 1.16 identity checks.",
            "Provide the local Ghidra analyzeHeadless executable to generate inventories.",
        ]
        _atomic_json(status_path, status, overwrite=True)
        return 0, status

    analyze_headless = Path(analyze_headless)
    if not analyze_headless.is_file():
        status["status"] = "BLOCKED_GHIDRA_EXECUTABLE_NOT_FOUND"
        status["errors"] = [f"Ghidra analyzeHeadless executable not found: {analyze_headless}"]
        _atomic_json(status_path, status, overwrite=True)
        return 2, status

    assert campaign_elf is not None and multiplayer_elf is not None
    campaign_report = inspect_file(Path(campaign_elf), "BLUS31270", "1.16")
    multiplayer_report = inspect_file(Path(multiplayer_elf), "BLUS31270", "1.16")
    campaign_range = campaign_report["elf"]["opd"]["operator_seed_range_candidate"]
    multiplayer_range = multiplayer_report["elf"]["opd"]["operator_seed_range_candidate"]
    if campaign_range is None or multiplayer_range is None:
        status["status"] = "BLOCKED_OPD_RANGE_NOT_VERIFIED"
        status["errors"] = [
            "Campaign or Multiplayer ELF has no structurally valid OPD seed-range candidate.",
            "The pipeline does not guess OPD addresses or sizes.",
        ]
        status["elf_inspection"] = {
            "campaign": _elf_summary(campaign_report),
            "multiplayer": _elf_summary(multiplayer_report),
        }
        _atomic_json(status_path, status, overwrite=True)
        return 2, status

    with tempfile.TemporaryDirectory(prefix="ghosts-tu116-inspect-") as temporary:
        temporary_dir = Path(temporary)
        campaign_inspector = temporary_dir / "campaign_elf_inspector.json"
        multiplayer_inspector = temporary_dir / "multiplayer_elf_inspector.json"
        campaign_inspector.write_text(json.dumps(campaign_report, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        multiplayer_inspector.write_text(json.dumps(multiplayer_report, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        try:
            manifest = generate_inventories(
                analyze_headless, Path(campaign_elf), Path(multiplayer_elf), output_dir,
                script_dir=script_dir,
                campaign_opd_inspector_report=campaign_inspector,
                multiplayer_opd_inspector_report=multiplayer_inspector,
                overwrite=overwrite,
            )
        except (OSError, ValueError, RuntimeError) as exc:
            status["status"] = "GHIDRA_ANALYSIS_FAILED"
            status["errors"] = [str(exc)]
            status["elf_inspection"] = {
                "campaign": _elf_summary(campaign_report),
                "multiplayer": _elf_summary(multiplayer_report),
            }
            _atomic_json(status_path, status, overwrite=True)
            return 2, status

    manifest_path = output_dir / OUTPUT_NAMES[5]
    status.update({
        "status": "NATIVE_PPU_INVENTORIES_GENERATED",
        "ghidra_invoked": True,
        "campaign_elf_sha256": campaign_report["input"]["sha256"],
        "multiplayer_elf_sha256": multiplayer_report["input"]["sha256"],
        "elf_inspection": {
            "campaign": _elf_summary(campaign_report),
            "multiplayer": _elf_summary(multiplayer_report),
        },
        "ppu_inventory_manifest": manifest_path.name,
        "ppu_inventory_manifest_sha256": _sha256(manifest_path),
        "generated_inventory_files": list(OUTPUT_NAMES),
        "campaign_function_records": manifest["campaign"]["function_records"],
        "multiplayer_function_records": manifest["multiplayer"]["function_records"],
        "multiplayer_only_builtin_lead_count": manifest["multiplayer_only_builtin_lead_count"],
        "limitations": [
            "Ghidra output is static navigation evidence; it does not prove runtime Player 2 initialization.",
            "No hook address is approved and no game file is modified.",
            "A playable Campaign co-op build still requires native initialization and two-controller runtime validation.",
        ],
    })
    _atomic_json(status_path, status, overwrite=True)
    return 0, status


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--disc-root", type=Path, required=True)
    parser.add_argument("--update-root", type=Path, required=True)
    parser.add_argument("--campaign-elf", type=Path)
    parser.add_argument("--multiplayer-elf", type=Path)
    parser.add_argument("--analyze-headless", type=Path)
    parser.add_argument("--script-dir", type=Path)
    parser.add_argument("--output-dir", type=Path, default=Path("analysis/ghosts-tu116-native"))
    parser.add_argument("--overwrite", action="store_true")
    args = parser.parse_args(argv)
    try:
        code, report = run_pipeline(
            args.disc_root, args.update_root, args.output_dir,
            campaign_elf=args.campaign_elf,
            multiplayer_elf=args.multiplayer_elf,
            analyze_headless=args.analyze_headless,
            script_dir=args.script_dir,
            overwrite=args.overwrite,
        )
    except (OSError, ValueError) as exc:
        print(f"native analysis preflight failed: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True))
    return code


if __name__ == "__main__":
    raise SystemExit(main())
