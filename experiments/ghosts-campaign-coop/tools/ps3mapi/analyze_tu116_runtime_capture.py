#!/usr/bin/env python3
"""Run the complete read-only TU 1.16 Campaign/Multiplayer capture analysis."""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
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
    overwrite: bool = False,
) -> dict:
    """Verify both captures, reconstruct analysis ELFs, and compare evidence.

    The function never patches, decrypts, uploads, or modifies game files.
    Existing output files are protected unless overwrite=True is explicit.
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

    output_dir.mkdir(parents=True, exist_ok=True)
    outputs = {name: output_dir / name for name in OUTPUT_NAMES}
    existing = [path for path in outputs.values() if path.exists()]
    if existing and not overwrite:
        names = ", ".join(str(path) for path in existing)
        raise FileExistsError(
            f"analysis outputs already exist: {names}; pass --overwrite to replace them"
        )

    campaign_elf = outputs["campaign_tu116_runtime.elf"]
    multiplayer_elf = outputs["multiplayer_tu116_runtime.elf"]

    campaign_verification = verify_dump(
        campaign_self,
        campaign_dumps,
        map_file,
        campaign_elf,
        "campaign",
        overwrite=overwrite,
    )
    multiplayer_verification = verify_dump(
        multiplayer_self,
        multiplayer_dumps,
        map_file,
        multiplayer_elf,
        "multiplayer",
        overwrite=overwrite,
    )

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

    dump_checks_passed = (
        campaign_verification.get("status") == "PASS_METADATA"
        and multiplayer_verification.get("status") == "PASS_METADATA"
    )
    pad_gate_passed = (
        pad_comparison.get("status")
        == "BOTH_CONTROLLERS_SIMULTANEOUSLY_VISIBLE_IN_BOTH"
    )

    if pad_gate_passed:
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
            "mode_labels_are_operator_selected": pad_comparison.get(
                "mode_labels_are_operator_selected"
            ),
        },
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
        "outputs": {key: str(path) for key, path in outputs.items()},
        "next_gate": (
            "Review the reconstructed Campaign and Multiplayer PPU images in Ghidra, "
            "map verified local-client allocation and controller routing, and compare "
            "player-state/view ownership before designing any Campaign hook."
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
    return summary


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--campaign-self", type=Path, required=True)
    parser.add_argument("--campaign-dumps", type=Path, required=True)
    parser.add_argument("--multiplayer-self", type=Path, required=True)
    parser.add_argument("--multiplayer-dumps", type=Path, required=True)
    parser.add_argument("--campaign-log", type=Path)
    parser.add_argument("--multiplayer-log", type=Path)
    parser.add_argument("--map", dest="map_file", type=Path, default=DEFAULT_MAP)
    parser.add_argument("--output-dir", type=Path, default=Path("analysis/tu116-capture"))
    parser.add_argument("--overwrite", action="store_true")
    args = parser.parse_args(argv)

    try:
        result = analyze(
            args.campaign_self,
            args.campaign_dumps,
            args.multiplayer_self,
            args.multiplayer_dumps,
            args.output_dir,
            map_file=args.map_file,
            campaign_log=args.campaign_log,
            multiplayer_log=args.multiplayer_log,
            overwrite=args.overwrite,
        )
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    print(f"STATUS: {result['status']}")
    print(f"Player 2 gate: {result['player_two_gate']['status']}")
    print(f"Report: {Path(args.output_dir) / 'capture_summary.json'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
