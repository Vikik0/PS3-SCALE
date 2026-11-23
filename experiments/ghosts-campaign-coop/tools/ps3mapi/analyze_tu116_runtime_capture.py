#!/usr/bin/env python3
"""Run the complete read-only TU 1.16 Campaign/Multiplayer capture analysis."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[2]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from tools.compare_pad_probe_logs import compare as compare_pad_probe_logs
from tools.compare_pad_probe_logs import parse_log as parse_pad_probe_log
from tools.ps3mapi.compare_runtime_strings import compare as compare_runtime_strings
from tools.ps3mapi.diff_runtime_elfs import compare as compare_runtime_elfs
from tools.ps3mapi.scan_runtime_strings import scan as scan_runtime_strings
from tools.ps3mapi.verify_tu116_dump import verify as verify_dump
from tools.run_ghidra_ppu_inventory import _validate_opd_pair, generate_inventories

DEFAULT_MAP = Path(__file__).with_name("SELF_MEMORY_MAP_TU116.json")

OUTPUT_NAMES = (
    "campaign_tu116_runtime.elf",
    "multiplayer_tu116_runtime.elf",
    "campaign_dump_verification.json",
    "multiplayer_dump_verification.json",
    "campaign_runtime_strings.json",
    "multiplayer_runtime_strings.json",
    "runtime_string_comparison.json",
    "runtime_binary_comparison.json",
    "pad_probe_comparison.json",
    "capture_summary.json",
)
GHIDRA_OUTPUT_MAP = {
    "campaign_ppu_inventory.csv": "ppu_campaign_ppu_inventory.csv",
    "multiplayer_ppu_inventory.csv": "ppu_multiplayer_ppu_inventory.csv",
    "campaign_builtin_xrefs.csv": "ppu_campaign_builtin_xrefs.csv",
    "multiplayer_builtin_xrefs.csv": "ppu_multiplayer_builtin_xrefs.csv",
    "splitscreen_builtin_xrefs.json": "ppu_splitscreen_builtin_xrefs.json",
    "ppu_inventory_manifest.json": "ppu_inventory_manifest.json",
}
GHIDRA_OUTPUT_NAMES = tuple(GHIDRA_OUTPUT_MAP.values())


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(
        json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    temporary.replace(path)


def _require_file(path: Path, label: str) -> None:
    if not path.is_file():
        raise FileNotFoundError(f"{label} was not found: {path}")


def _require_directory(path: Path, label: str) -> None:
    if not path.is_dir():
        raise FileNotFoundError(f"{label} directory was not found: {path}")


def _commit_staged_outputs(
    staged_outputs: dict[str, Path],
    final_outputs: dict[str, Path],
    *,
    overwrite: bool,
    output_names: tuple[str, ...] = OUTPUT_NAMES,
) -> None:
    """Promote a complete analysis set, restoring old outputs if promotion fails."""
    for name in output_names:
        staged = staged_outputs[name]
        if not staged.is_file():
            raise FileNotFoundError(f"staged analysis output is missing: {staged}")

    output_dir = next(iter(final_outputs.values())).parent
    backup_dir = Path(tempfile.mkdtemp(prefix=".ghosts-capture-backup-", dir=output_dir))
    keep_backup = False
    moved_existing: list[tuple[Path, Path]] = []
    promoted: list[Path] = []

    try:
        for name in output_names:
            staged = staged_outputs[name]
            target = final_outputs[name]

            if target.exists():
                if not overwrite:
                    raise FileExistsError(
                        f"analysis output appeared during the run: {target}; "
                        "pass --overwrite to replace it"
                    )
                if not target.is_file():
                    raise IsADirectoryError(f"analysis output path is not a file: {target}")
                backup = backup_dir / name
                os.replace(target, backup)
                moved_existing.append((backup, target))

            os.replace(staged, target)
            promoted.append(target)
    except Exception as exc:
        rollback_errors: list[OSError] = []
        for target in reversed(promoted):
            try:
                if target.exists():
                    target.unlink()
            except OSError as rollback_error:
                rollback_errors.append(rollback_error)

        for backup, target in reversed(moved_existing):
            try:
                if backup.exists():
                    os.replace(backup, target)
            except OSError as rollback_error:
                rollback_errors.append(rollback_error)

        if rollback_errors:
            keep_backup = True
            raise RuntimeError(
                "analysis output promotion failed and rollback was incomplete; "
                f"preserve recovery files at {backup_dir}"
            ) from rollback_errors[0]
        raise
    finally:
        if not keep_backup:
            shutil.rmtree(backup_dir, ignore_errors=True)


def analyze(
    campaign_self: Path,
    campaign_dumps: Path,
    multiplayer_self: Path,
    multiplayer_dumps: Path,
    output_dir: Path,
    *,
    map_file: Path = DEFAULT_MAP,
    campaign_log: Path | None = None,
    multiplayer_log: Path | None = None,
    analyze_headless: Path | None = None,
    ghidra_script_dir: Path | None = None,
    campaign_opd: tuple[int, int] | None = None,
    multiplayer_opd: tuple[int, int] | None = None,
    overwrite: bool = False,
) -> dict:
    """Verify both captures, reconstruct analysis ELFs, and compare evidence.

    The function never patches, decrypts, uploads, or modifies game files.
    The base ten outputs are always staged; six PPU outputs are added when Ghidra
    is enabled. Existing outputs are replaced only after every requested step
    succeeds, and are restored if the promotion fails.
    """
    campaign_self = Path(campaign_self)
    campaign_dumps = Path(campaign_dumps)
    multiplayer_self = Path(multiplayer_self)
    multiplayer_dumps = Path(multiplayer_dumps)
    output_dir = Path(output_dir)
    map_file = Path(map_file)

    _require_file(campaign_self, "Campaign default.self")
    _require_file(multiplayer_self, "Multiplayer default_mp.self")
    _require_directory(campaign_dumps, "Campaign dump")
    _require_directory(multiplayer_dumps, "Multiplayer dump")
    _require_file(map_file, "TU 1.16 memory map")

    if campaign_self.resolve() == multiplayer_self.resolve():
        raise ValueError("Campaign and Multiplayer SELF inputs must be different files")
    if campaign_dumps.resolve() == multiplayer_dumps.resolve():
        raise ValueError("Campaign and Multiplayer dump directories must be different")
    if (campaign_log is None) != (multiplayer_log is None):
        raise ValueError("provide both pad-probe logs or neither")

    if campaign_log is not None:
        campaign_log = Path(campaign_log)
        multiplayer_log = Path(multiplayer_log)
        _require_file(campaign_log, "Campaign Pad Probe log")
        _require_file(multiplayer_log, "Multiplayer Pad Probe log")

    if analyze_headless is not None:
        analyze_headless = Path(analyze_headless)
        _require_file(analyze_headless, "Ghidra analyzeHeadless executable")
    elif ghidra_script_dir is not None or campaign_opd is not None or multiplayer_opd is not None:
        raise ValueError("Ghidra script directory and OPD ranges require --analyze-headless")

    output_names = OUTPUT_NAMES + GHIDRA_OUTPUT_NAMES if analyze_headless is not None else OUTPUT_NAMES
    output_dir.mkdir(parents=True, exist_ok=True)
    final_outputs = {name: output_dir / name for name in output_names}
    existing = [path for path in final_outputs.values() if path.exists()]
    if existing and not overwrite:
        names = ", ".join(str(path) for path in existing)
        raise FileExistsError(
            f"analysis outputs already exist: {names}; pass --overwrite to replace them"
        )

    with tempfile.TemporaryDirectory(
        prefix=".ghosts-capture-stage-", dir=output_dir
    ) as staging:
        stage_dir = Path(staging)
        outputs = {name: stage_dir / name for name in output_names}

        campaign_elf = outputs["campaign_tu116_runtime.elf"]
        multiplayer_elf = outputs["multiplayer_tu116_runtime.elf"]

        campaign_verification = verify_dump(
            campaign_self,
            campaign_dumps,
            map_file,
            campaign_elf,
            "campaign",
            overwrite=False,
        )
        multiplayer_verification = verify_dump(
            multiplayer_self,
            multiplayer_dumps,
            map_file,
            multiplayer_elf,
            "multiplayer",
            overwrite=False,
        )

        # The verification JSON must reference the final paths, not the temporary stage.
        campaign_rebuilt = campaign_verification.get("reconstructed_elf")
        if isinstance(campaign_rebuilt, dict):
            campaign_rebuilt["output_elf"] = str(final_outputs["campaign_tu116_runtime.elf"])
        multiplayer_rebuilt = multiplayer_verification.get("reconstructed_elf")
        if isinstance(multiplayer_rebuilt, dict):
            multiplayer_rebuilt["output_elf"] = str(final_outputs["multiplayer_tu116_runtime.elf"])

        campaign_strings = scan_runtime_strings(campaign_elf)
        multiplayer_strings = scan_runtime_strings(multiplayer_elf)
        string_comparison = compare_runtime_strings(campaign_elf, multiplayer_elf)
        binary_comparison = compare_runtime_elfs(campaign_elf, multiplayer_elf)

        if campaign_log is None:
            pad_comparison = {
                "status": "NOT_PROVIDED",
                "both_controllers_simultaneously_visible_in_both": False,
                "interpretation_limit": (
                    "No Pad Probe logs were supplied. Runtime ELF analysis can proceed, "
                    "but controller-port visibility has not been assessed."
                ),
            }
        else:
            pad_comparison = compare_pad_probe_logs(
                parse_pad_probe_log(campaign_log),
                parse_pad_probe_log(multiplayer_log),
            )
            pad_comparison["input_sha256"] = {
                "campaign_log": sha256_file(campaign_log),
                "multiplayer_log": sha256_file(multiplayer_log),
            }

        ppu_analysis = {"status": "NOT_RUN"}
        if analyze_headless is not None:
            ghidra_output_dir = stage_dir / "ghidra-ppu"
            ghidra_manifest = generate_inventories(
                analyze_headless,
                campaign_elf,
                multiplayer_elf,
                ghidra_output_dir,
                script_dir=ghidra_script_dir,
                campaign_opd=campaign_opd,
                multiplayer_opd=multiplayer_opd,
            )
            ghidra_manifest_path = ghidra_output_dir / "ppu_inventory_manifest.json"
            manifest_data = json.loads(ghidra_manifest_path.read_text(encoding="utf-8"))
            manifest_data["campaign"]["elf_path"] = str(final_outputs["campaign_tu116_runtime.elf"])
            manifest_data["multiplayer"]["elf_path"] = str(final_outputs["multiplayer_tu116_runtime.elf"])
            write_json(ghidra_manifest_path, manifest_data)
            for source_name, output_name in GHIDRA_OUTPUT_MAP.items():
                source_path = ghidra_output_dir / source_name
                if not source_path.is_file():
                    raise FileNotFoundError(f"Ghidra evidence output is missing: {source_path}")
                shutil.copyfile(source_path, outputs[output_name])
            comparison = json.loads(
                (ghidra_output_dir / "splitscreen_builtin_xrefs.json").read_text(encoding="utf-8")
            )
            ppu_analysis = {
                "status": "GHIDRA_PPU_ANALYSIS_COMPLETED",
                "campaign_function_records": manifest_data["campaign"]["function_records"],
                "multiplayer_function_records": manifest_data["multiplayer"]["function_records"],
                "campaign_builtin_xref_rows": manifest_data["campaign"]["builtin_xref_rows"],
                "multiplayer_builtin_xref_rows": manifest_data["multiplayer"]["builtin_xref_rows"],
                "multiplayer_only_builtin_lead_count": len(comparison["multiplayer_only_leads"]),
                "comparison_sha256": sha256_file(ghidra_output_dir / "splitscreen_builtin_xrefs.json"),
                "manifest_sha256": sha256_file(ghidra_manifest_path),
            }

        dump_checks_passed = (
            campaign_verification.get("status") == "PASS_METADATA"
            and multiplayer_verification.get("status") == "PASS_METADATA"
        )
        pad_gate_passed = (
            pad_comparison.get("status")
            == "BOTH_CONTROLLERS_SIMULTANEOUSLY_VISIBLE_IN_BOTH"
        )

        if not dump_checks_passed:
            status = "CAPTURE_VERIFICATION_FAILED"
        elif pad_gate_passed:
            status = "CAPTURE_READY_FOR_ENGINE_REVIEW"
        elif campaign_log is None:
            status = "ELF_ANALYSIS_READY_PAD_EVIDENCE_MISSING"
        else:
            status = "ELF_ANALYSIS_READY_PAD_GATE_NOT_PASSED"

        write_json(outputs["campaign_dump_verification.json"], campaign_verification)
        write_json(outputs["multiplayer_dump_verification.json"], multiplayer_verification)
        write_json(outputs["campaign_runtime_strings.json"], campaign_strings)
        write_json(outputs["multiplayer_runtime_strings.json"], multiplayer_strings)
        write_json(outputs["runtime_string_comparison.json"], string_comparison)
        write_json(outputs["runtime_binary_comparison.json"], binary_comparison)
        write_json(outputs["pad_probe_comparison.json"], pad_comparison)

        input_hashes = {
            "campaign_default_self": sha256_file(campaign_self),
            "multiplayer_default_mp_self": sha256_file(multiplayer_self),
        }
        if campaign_log is not None:
            input_hashes["campaign_pad_probe_log"] = sha256_file(campaign_log)
            input_hashes["multiplayer_pad_probe_log"] = sha256_file(multiplayer_log)

        summary = {
            "status": status,
            "profile": {
                "title_id": "BLUS31270",
                "title_update": "1.16",
                "target_console": "PS3 CEX CFW 4.93",
                "player_count_target": 2,
                "mode": "offline local Campaign co-op",
            },
            "dump_verification": {
                "campaign": campaign_verification.get("status"),
                "multiplayer": multiplayer_verification.get("status"),
                "both_passed": dump_checks_passed,
            },
            "pad_probe": {
                "status": pad_comparison.get("status"),
                "both_ports_visible_simultaneously_in_both_modes": pad_gate_passed,
                "capture_complete_in_both_modes": pad_comparison.get(
                    "capture_complete_in_both_modes", False
                ),
                "capture_provenance_valid_in_both_modes": pad_comparison.get(
                    "capture_provenance_valid_in_both_modes", False
                ),
                "mode_labels_are_operator_selected": pad_comparison.get(
                    "mode_labels_are_operator_selected"
                ),
            },
            "ppu_analysis": ppu_analysis,
            "player_two_gate": {
                "status": "NOT_PROVEN",
                "native_campaign_local_client_creation": False,
                "controller_to_player_two_mapping": False,
                "independent_camera_and_hud": False,
                "reason": (
                    "These require verified runtime call-path and player-state evidence. "
                    "Pad visibility, strings, or changed binary blocks are not sufficient."
                ),
            },
            "input_sha256": input_hashes,
            "outputs": {key: str(path) for key, path in final_outputs.items()},
            "next_gate": (
                "Review the PPU builtin-XREF comparison and inspect each Multiplayer-only lead in Ghidra; "
                "then verify local-client allocation, controller routing and player-state/view ownership "
                "before designing any Campaign hook."
                if analyze_headless is not None else
                "Run the one-shot Ghidra PPU analysis on these reconstructed ELFs to map verified local-client "
                "allocation, controller routing and player-state/view ownership before designing any Campaign hook."
            ),
            "limitations": [
                "PASS_METADATA means the SELF headers and captured PT_LOAD sizes match the recorded TU 1.16 profile; it is not cryptographic proof of the installed update.",
                "The operator selects Campaign or Multiplayer manually. The dump and Pad Probe tools do not auto-detect the active game mode.",
                "A 0x03 port mask proves only that the pad subsystem reported ports 0 and 1 together in one sample.",
                "String matches and changed ELF blocks are navigation leads, not function boundaries or approved hook addresses.",
                "The reconstructed ELF is for static analysis only and is not a runnable EBOOT.",
                "This pipeline does not spawn Player 2, patch the game, rebuild signed FastFiles, or create an installable/playable PKG.",
            ],
        }
        write_json(outputs["capture_summary.json"], summary)
        _commit_staged_outputs(
            outputs, final_outputs, overwrite=overwrite, output_names=output_names
        )

    return summary


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--campaign-self", type=Path, required=True)
    parser.add_argument("--campaign-dumps", type=Path, required=True)
    parser.add_argument("--multiplayer-self", type=Path, required=True)
    parser.add_argument("--multiplayer-dumps", type=Path, required=True)
    parser.add_argument("--campaign-log", type=Path)
    parser.add_argument("--multiplayer-log", type=Path)
    parser.add_argument("--analyze-headless", type=Path)
    parser.add_argument("--ghidra-script-dir", type=Path)
    parser.add_argument("--campaign-opd-va")
    parser.add_argument("--campaign-opd-size")
    parser.add_argument("--multiplayer-opd-va")
    parser.add_argument("--multiplayer-opd-size")
    parser.add_argument("--map", dest="map_file", type=Path, default=DEFAULT_MAP)
    parser.add_argument("--output-dir", type=Path, default=Path("analysis/tu116-capture"))
    parser.add_argument("--overwrite", action="store_true")
    args = parser.parse_args(argv)

    try:
        campaign_opd = _validate_opd_pair(
            args.campaign_opd_va, args.campaign_opd_size, "Campaign"
        )
        multiplayer_opd = _validate_opd_pair(
            args.multiplayer_opd_va, args.multiplayer_opd_size, "Multiplayer"
        )
        result = analyze(
            args.campaign_self,
            args.campaign_dumps,
            args.multiplayer_self,
            args.multiplayer_dumps,
            args.output_dir,
            map_file=args.map_file,
            campaign_log=args.campaign_log,
            multiplayer_log=args.multiplayer_log,
            analyze_headless=args.analyze_headless,
            ghidra_script_dir=args.ghidra_script_dir,
            campaign_opd=campaign_opd,
            multiplayer_opd=multiplayer_opd,
            overwrite=args.overwrite,
        )
    except (OSError, ValueError, KeyError, json.JSONDecodeError, subprocess.CalledProcessError, RuntimeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    print(f"STATUS: {result['status']}")
    print(f"Player 2 gate: {result['player_two_gate']['status']}")
    print(f"Report: {Path(args.output_dir) / 'capture_summary.json'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
