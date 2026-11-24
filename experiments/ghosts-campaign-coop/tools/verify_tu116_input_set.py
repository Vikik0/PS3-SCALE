#!/usr/bin/env python3
"""Verify the exact user-owned Ghosts BLUS31270 TU 1.16 static input set.

This is a read-only SFO/SELF metadata check. It does not decrypt executable
segments, create runtime memory dumps, patch files, or claim a playable mod.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path
from typing import Any

PROJECT_ROOT = Path(__file__).resolve().parents[1]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from tools.ps3_self_inspect import SelfFormatError, inspect_self
from tools.ps3_sfo_inspect import SfoFormatError, inspect_file as inspect_sfo

DEFAULT_MANIFEST = Path(__file__).with_name("ps3mapi") / "TU116_EXECUTABLE_MANIFEST.json"
DEFAULT_MEMORY_MAP = Path(__file__).with_name("ps3mapi") / "SELF_MEMORY_MAP_TU116.json"


def _load_json(path: Path, label: str) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except OSError as exc:
        raise ValueError(f"{label} could not be read: {exc}") from exc
    except json.JSONDecodeError as exc:
        raise ValueError(f"{label} is not valid JSON: {exc}") from exc
    if not isinstance(value, dict):
        raise ValueError(f"{label} must be a JSON object")
    return value


def _address_string(value: Any) -> str:
    if isinstance(value, str):
        return f"0x{int(value, 0):X}"
    return f"0x{int(value):X}"


def _segment_tuple(segment: dict[str, Any]) -> tuple[int, str, int, int, int]:
    return (
        int(segment["segment_index"]),
        _address_string(segment["virtual_address"]),
        int(segment["file_size"]),
        int(segment["memory_size"]),
        int(segment["alignment"]),
    )


def _actual_segment_tuple(segment: dict[str, Any]) -> tuple[int, str, int, int, int]:
    return (
        int(segment["index"]),
        _address_string(segment["virtual_address"]),
        int(segment["elf_file_size"]),
        int(segment["memory_size"]),
        int(segment["alignment"]),
    )


def _verify_executable(
    path: Path,
    label: str,
    expected: dict[str, Any],
    expected_segments: list[dict[str, Any]] | None = None,
) -> tuple[dict[str, Any] | None, list[str]]:
    errors: list[str] = []
    try:
        data = path.read_bytes()
        report = inspect_self(data, path.name)
    except (OSError, SelfFormatError) as exc:
        return None, [f"{label}: {exc}"]

    if len(data) != int(expected["size_bytes"]):
        errors.append(f"{label}: size mismatch ({len(data)} != {expected['size_bytes']})")
    if report["sha256"].lower() != str(expected["sha256"]).lower():
        errors.append(f"{label}: SHA-256 mismatch")
    if report["container"]["key_revision"] != int(expected["key_revision"]):
        errors.append(f"{label}: SELF key revision mismatch")
    if report["embedded_elf_header"]["entry_point"].upper() != str(expected["entry_point"]).upper():
        errors.append(f"{label}: ELF entry point mismatch")
    if report["embedded_elf_header"]["machine"] != "PowerPC64":
        errors.append(f"{label}: ELF machine is not PowerPC64")
    if report["embedded_elf_header"]["class"] != "ELF64" or report["embedded_elf_header"]["endianness"] != "big":
        errors.append(f"{label}: ELF format is not 64-bit big-endian")
    if not report["segment_summary"]["all_container_ranges_valid"]:
        errors.append(f"{label}: SELF container segment range validation failed")
    if int(report["segment_summary"]["encrypted_segments"]) < 1:
        errors.append(f"{label}: expected retail encrypted segments were not detected")

    if expected_segments is not None:
        actual = [
            item for item in report["segments"]
            if int(item["program_type"]) == 1 and int(item["elf_file_size"]) > 0
        ]
        actual_tuples = [_actual_segment_tuple(item) for item in actual]
        expected_tuples = [_segment_tuple(item) for item in expected_segments]
        if actual_tuples != expected_tuples:
            errors.append(f"{label}: non-empty PT_LOAD segment map mismatch")

    return report, errors


def verify_input_set(
    sfo_path: Path,
    eboot_path: Path,
    campaign_self_path: Path,
    multiplayer_self_path: Path,
    manifest: dict[str, Any],
    memory_map: dict[str, Any],
) -> dict[str, Any]:
    errors: list[str] = []
    sfo_report: dict[str, Any] | None = None
    try:
        sfo_report = inspect_sfo(Path(sfo_path))
    except (OSError, SfoFormatError) as exc:
        errors.append(f"PARAM.SFO: {exc}")

    target = manifest["target"]
    if sfo_report is not None:
        entries = sfo_report.get("entries", {})
        if entries.get("TITLE_ID") != target["title_id"]:
            errors.append("PARAM.SFO: TITLE_ID does not match the selected target")
        if entries.get("APP_VER") != target["app_ver"]:
            errors.append("PARAM.SFO: APP_VER does not match the selected Title Update")
        if entries.get("PS3_SYSTEM_VER") != target["ps3_system_ver"]:
            errors.append("PARAM.SFO: PS3_SYSTEM_VER mismatch")
        if entries.get("TITLE") != target["title"]:
            errors.append("PARAM.SFO: title string mismatch")
        if sfo_report["input"]["sha256"].lower() != manifest["sfo"]["sha256"].lower():
            errors.append("PARAM.SFO: SHA-256 mismatch")

    campaign_path = Path(campaign_self_path)
    multiplayer_path = Path(multiplayer_self_path)
    if campaign_path.resolve() == multiplayer_path.resolve():
        errors.append("Campaign and Multiplayer SELF inputs must be different files")

    eboot_report, eboot_errors = _verify_executable(
        Path(eboot_path), "EBOOT", manifest["executables"]["eboot"]
    )
    errors.extend(eboot_errors)

    campaign_expected = manifest["executables"]["campaign"]
    campaign_map = memory_map.get("campaign", {})
    if campaign_map.get("entry_point", "").upper() != campaign_expected["entry_point"].upper():
        errors.append("memory map: Campaign entry point differs from the executable manifest")
    if [_segment_tuple(x) for x in campaign_map.get("pt_load_segments", [])] != [
        _segment_tuple(x) for x in campaign_expected["pt_load_segments"]
    ]:
        errors.append("memory map: Campaign PT_LOAD map differs from the executable manifest")
    campaign_report, campaign_errors = _verify_executable(
        campaign_path, "Campaign SELF", campaign_expected, campaign_map.get("pt_load_segments", [])
    )
    errors.extend(campaign_errors)

    multiplayer_expected = manifest["executables"]["multiplayer"]
    multiplayer_map = memory_map.get("multiplayer", {})
    if multiplayer_map.get("entry_point", "").upper() != multiplayer_expected["entry_point"].upper():
        errors.append("memory map: Multiplayer entry point differs from the executable manifest")
    if [_segment_tuple(x) for x in multiplayer_map.get("pt_load_segments", [])] != [
        _segment_tuple(x) for x in multiplayer_expected["pt_load_segments"]
    ]:
        errors.append("memory map: Multiplayer PT_LOAD map differs from the executable manifest")
    multiplayer_report, multiplayer_errors = _verify_executable(
        multiplayer_path, "Multiplayer SELF", multiplayer_expected, multiplayer_map.get("pt_load_segments", [])
    )
    errors.extend(multiplayer_errors)

    return {
        "schema_version": 1,
        "status": "PASS_STATIC_INPUTS" if not errors else "FAIL_STATIC_INPUTS",
        "ok": not errors,
        "target": target,
        "inputs": {
            "sfo": sfo_report,
            "eboot": eboot_report,
            "campaign_self": campaign_report,
            "multiplayer_self": multiplayer_report,
        },
        "errors": errors,
        "runtime_capture": {
            "campaign_pt_load_dumps": "NOT_PROVIDED",
            "multiplayer_pt_load_dumps": "NOT_PROVIDED",
            "campaign_pad_probe_log": "NOT_PROVIDED",
            "multiplayer_pad_probe_log": "NOT_PROVIDED",
            "ghidra_ppu_inventory": "NOT_RUN",
            "playable_campaign_player_two": "NOT_PROVEN",
        },
        "limitations": [
            "Static SFO/SELF metadata verification only; encrypted executable segments are not decrypted.",
            "This report does not prove the installed console build or a live PS3 runtime capture.",
            "No local Campaign client, controller route, independent camera or HUD is created by this tool.",
        ],
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sfo", type=Path, required=True, help="the update PARAM.SFO")
    parser.add_argument("--eboot", type=Path, required=True, help="the matching EBOOT.BIN")
    parser.add_argument("--campaign-self", type=Path, required=True, help="the TU 1.16 default.self")
    parser.add_argument("--multiplayer-self", type=Path, required=True, help="the TU 1.16 default_mp.self")
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--memory-map", type=Path, default=DEFAULT_MEMORY_MAP)
    parser.add_argument("--output-json", type=Path, help="optional output report path")
    args = parser.parse_args(argv)

    try:
        manifest = _load_json(args.manifest, "executable manifest")
        memory_map = _load_json(args.memory_map, "TU 1.16 memory map")
        report = verify_input_set(
            args.sfo, args.eboot, args.campaign_self, args.multiplayer_self,
            manifest, memory_map,
        )
    except (OSError, ValueError, KeyError, TypeError) as exc:
        print(f"TU 1.16 input verification failed: {exc}", file=sys.stderr)
        return 2

    rendered = json.dumps(report, indent=2, ensure_ascii=False) + "\n"
    if args.output_json:
        args.output_json.write_text(rendered, encoding="utf-8")
    print(rendered, end="")
    return 0 if report["ok"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
