#!/usr/bin/env python3
"""Validate operator-recorded PS3MAPI provenance for one TU 1.16 capture."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from datetime import datetime
from pathlib import Path

EXPECTED_PROFILE = {
    "profile_title_id": "BLUS31270",
    "profile_app_ver": "01.16",
}
EXPECTED_FILES = {
    "campaign": {
        "mode": "CAMPAIGN",
        "pad_log": "ghosts_campaign_pad_probe.log",
        "segment0": "GHOSTS_TU116_CAMPAIGN_SEG0.bin",
        "segment1": "GHOSTS_TU116_CAMPAIGN_SEG1.bin",
    },
    "multiplayer": {
        "mode": "MULTIPLAYER",
        "pad_log": "ghosts_multiplayer_pad_probe.log",
        "segment0": "GHOSTS_TU116_MULTIPLAYER_SEG0.bin",
        "segment1": "GHOSTS_TU116_MULTIPLAYER_SEG1.bin",
    },
}
REQUIRED_KEYS = (
    "profile_title_id",
    "profile_app_ver",
    "operator_selected_mode",
    "capture_date_local",
    "ps3mapi_current_pid",
    "ps3mapi_process_name_verbatim",
    "pad_probe_log_filename",
    "segment0_filename",
    "segment1_filename",
)
ALLOWED_KEYS = set(REQUIRED_KEYS) | {"notes"}
KEY_RE = re.compile(r"^[a-z][a-z0-9_]*$")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def parse_metadata(path: Path) -> dict[str, str]:
    path = Path(path)
    if not path.is_file():
        raise FileNotFoundError(f"capture metadata file was not found: {path}")

    values: dict[str, str] = {}
    for line_number, raw_line in enumerate(
        path.read_text(encoding="utf-8-sig").splitlines(), start=1
    ):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            raise ValueError(f"{path}:{line_number}: expected key=value")
        raw_key, raw_value = line.split("=", 1)
        key = raw_key.strip()
        if not KEY_RE.fullmatch(key):
            raise ValueError(f"{path}:{line_number}: invalid metadata key {key!r}")
        if key in values:
            raise ValueError(f"{path}:{line_number}: duplicate metadata key {key!r}")
        if key not in ALLOWED_KEYS:
            raise ValueError(f"{path}:{line_number}: unknown metadata key {key!r}")
        values[key] = raw_value.strip()

    missing = [key for key in REQUIRED_KEYS if not values.get(key)]
    if missing:
        raise ValueError(
            f"{path}: missing required metadata values: {', '.join(missing)}"
        )
    return values


def verify_capture_metadata(
    metadata_file: Path,
    mode: str,
    dump_dir: Path,
    *,
    pad_log: Path | None = None,
) -> dict:
    """Validate one metadata file against the actual dump and optional probe log."""
    mode = mode.lower()
    if mode not in EXPECTED_FILES:
        raise ValueError("mode must be campaign or multiplayer")

    metadata_file = Path(metadata_file)
    dump_dir = Path(dump_dir)
    if not dump_dir.is_dir():
        raise FileNotFoundError(f"{mode} dump directory was not found: {dump_dir}")

    values = parse_metadata(metadata_file)
    expected = EXPECTED_FILES[mode]

    for key, wanted in EXPECTED_PROFILE.items():
        if values[key].upper() != wanted.upper():
            raise ValueError(
                f"{mode} metadata {key} mismatch: got {values[key]!r}, expected {wanted!r}"
            )

    if values["operator_selected_mode"].upper() != expected["mode"]:
        raise ValueError(
            f"{mode} metadata mode mismatch: got {values['operator_selected_mode']!r}, "
            f"expected {expected['mode']!r}"
        )

    try:
        datetime.strptime(values["capture_date_local"], "%Y-%m-%d %H:%M:%S")
    except ValueError as exc:
        raise ValueError(
            f"{mode} capture_date_local must use YYYY-MM-DD HH:MM:SS local time"
        ) from exc

    pid_text = values["ps3mapi_current_pid"]
    if not pid_text.isdecimal():
        raise ValueError(f"{mode} PS3MAPI PID must be a positive decimal integer")
    pid = int(pid_text, 10)
    if pid <= 0 or pid > 0xFFFFFFFF:
        raise ValueError(f"{mode} PS3MAPI PID is outside the valid unsigned 32-bit range")

    process_name = values["ps3mapi_process_name_verbatim"]
    if len(process_name) > 255 or any(ord(char) < 32 or ord(char) == 127 for char in process_name):
        raise ValueError(f"{mode} process name contains control characters or exceeds 255 characters")

    for key in ("pad_log", "segment0", "segment1"):
        if values[{
            "pad_log": "pad_probe_log_filename",
            "segment0": "segment0_filename",
            "segment1": "segment1_filename",
        }[key]].casefold() != expected[key].casefold():
            raise ValueError(
                f"{mode} metadata {key} filename mismatch: expected {expected[key]!r}"
            )

    segment_paths = {
        "segment0": dump_dir / expected["segment0"],
        "segment1": dump_dir / expected["segment1"],
    }
    for label, path in segment_paths.items():
        if not path.is_file():
            raise FileNotFoundError(
                f"{mode} metadata references {path.name}, but that file is missing"
            )

    if pad_log is not None:
        pad_log = Path(pad_log)
        if not pad_log.is_file():
            raise FileNotFoundError(f"{mode} Pad Probe log was not found: {pad_log}")
        if pad_log.name.casefold() != expected["pad_log"].casefold():
            raise ValueError(
                f"{mode} Pad Probe log filename mismatch: expected {expected['pad_log']!r}"
            )
        if values["pad_probe_log_filename"].casefold() != pad_log.name.casefold():
            raise ValueError(
                f"{mode} metadata Pad Probe filename does not match supplied log {pad_log.name!r}"
            )

    return {
        "status": "PASS_CAPTURE_METADATA",
        "mode": mode,
        "metadata_file": str(metadata_file),
        "metadata_sha256": sha256_file(metadata_file),
        "title_id": values["profile_title_id"].upper(),
        "app_ver": values["profile_app_ver"],
        "operator_selected_mode": expected["mode"],
        "capture_date_local": values["capture_date_local"],
        "ps3mapi_pid": pid,
        "ps3mapi_process_name_verbatim": process_name,
        "process_name_is_diagnostic_only": True,
        "pad_probe_log_filename": expected["pad_log"],
        "segment_files": {
            "0": {
                "filename": segment_paths["segment0"].name,
                "size_bytes": segment_paths["segment0"].stat().st_size,
                "sha256": sha256_file(segment_paths["segment0"]),
            },
            "1": {
                "filename": segment_paths["segment1"].name,
                "size_bytes": segment_paths["segment1"].stat().st_size,
                "sha256": sha256_file(segment_paths["segment1"]),
            },
        },
        "limitations": [
            "The mode label is operator-selected; metadata does not auto-detect Campaign or Multiplayer.",
            "PROCESS GETNAME is recorded verbatim for diagnostics and is not treated as a mode discriminator.",
            "File presence and hashes establish intake identity only; the TU 1.16 dump verifier must still validate sizes and SELF PT_LOAD metadata.",
        ],
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("campaign", "multiplayer"), required=True)
    parser.add_argument("--metadata", type=Path, required=True)
    parser.add_argument("--dumps", type=Path, required=True)
    parser.add_argument("--pad-log", type=Path)
    args = parser.parse_args(argv)
    try:
        result = verify_capture_metadata(
            args.metadata, args.mode, args.dumps, pad_log=args.pad_log
        )
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
