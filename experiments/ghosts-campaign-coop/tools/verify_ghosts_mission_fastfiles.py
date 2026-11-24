#!/usr/bin/env python3
"""Verify the exact user-supplied Ghosts mission FastFiles without modifying them."""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path
from typing import Any

EXPECTED_FASTFILES = {
    "black_ice": {
        "file_name": "black_ice.ff",
        "size_bytes": 91355961,
        "sha256": "950726399e16f55fd565438936ec59231649940c89763a87c805409536837532",
        "magic": "IWff0100",
        "version": "0x0000022E",
    },
    "carrier": {
        "file_name": "carrier.ff",
        "size_bytes": 87088544,
        "sha256": "e3a616545f18a7dcae006b4e32264c56b01d627650ea574f3fe710e02543caee",
        "magic": "IWff0100",
        "version": "0x0000022E",
    },
    "cornered": {
        "file_name": "cornered.ff",
        "size_bytes": 93933243,
        "sha256": "1bcb1da55d7e5a893de2a513e6df9b5cd3786e4d642dc76e090ea551cdf39395",
        "magic": "IWff0100",
        "version": "0x0000022E",
    },
}


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def verify_fastfiles(paths: dict[str, Path | str | None]) -> dict[str, Any]:
    errors: list[str] = []
    reports: dict[str, Any] = {}
    for role, expected in EXPECTED_FASTFILES.items():
        raw_path = paths.get(role)
        if raw_path is None:
            reports[role] = {"status": "MISSING", "expected_file_name": expected["file_name"]}
            errors.append(f"{role}: input path was not provided")
            continue

        path = Path(raw_path).expanduser()
        if not path.is_file():
            reports[role] = {
                "status": "MISSING",
                "file_name": path.name,
                "expected_file_name": expected["file_name"],
            }
            errors.append(f"{role}: FastFile does not exist: {path}")
            continue

        try:
            size_bytes = path.stat().st_size
            with path.open("rb") as stream:
                header = stream.read(12)
            magic = header[:8].decode("ascii", errors="replace")
            version = (
                f"0x{int.from_bytes(header[8:12], 'big'):08X}"
                if len(header) >= 12 else None
            )
            digest = _sha256(path)
        except OSError as exc:
            reports[role] = {"status": "UNREADABLE", "file_name": path.name}
            errors.append(f"{role}: unable to read FastFile: {exc}")
            continue

        checks = {
            "size_bytes": size_bytes == expected["size_bytes"],
            "sha256": digest == expected["sha256"],
            "magic": magic == expected["magic"],
            "version": version == expected["version"],
        }
        status = "MATCHED" if all(checks.values()) else "MISMATCH"
        reports[role] = {
            "status": status,
            "file_name": path.name,
            "size_bytes": size_bytes,
            "sha256": digest,
            "magic": magic,
            "version": version,
            "expected": {
                "file_name": expected["file_name"],
                "size_bytes": expected["size_bytes"],
                "sha256": expected["sha256"],
                "magic": expected["magic"],
                "version": expected["version"],
            },
            "checks": checks,
            "privacy": "File contents are not copied into this report.",
        }
        if status != "MATCHED":
            failed = ", ".join(key for key, passed in checks.items() if not passed)
            errors.append(f"{role}: FastFile identity mismatch ({failed})")

    extra_roles = sorted(set(paths) - set(EXPECTED_FASTFILES))
    if extra_roles:
        errors.append("unexpected FastFile roles: " + ", ".join(extra_roles))

    matched = len(reports) == len(EXPECTED_FASTFILES) and all(
        report.get("status") == "MATCHED" for report in reports.values()
    ) and not extra_roles
    return {
        "schema_version": 1,
        "tool": "verify_ghosts_mission_fastfiles",
        "status": "MATCHED_MISSION_FASTFILES" if matched else "MISMATCH_OR_INCOMPLETE",
        "files": reports,
        "errors": errors,
        "safety": [
            "Read-only identity verification; no FastFile bytes are changed.",
            "The report contains file names, sizes, hashes and magic only.",
            "A matching FastFile hash identifies the supplied bytes; it does not prove a playable co-op implementation.",
        ],
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--black-ice", type=Path, required=True)
    parser.add_argument("--carrier", type=Path, required=True)
    parser.add_argument("--cornered", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    report = verify_fastfiles({
        "black_ice": args.black_ice,
        "carrier": args.carrier,
        "cornered": args.cornered,
    })
    rendered = json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8")
        print(f"wrote FastFile identity report: {args.output}")
    else:
        sys.stdout.write(rendered)
    return 0 if report["status"] == "MATCHED_MISSION_FASTFILES" else 1


if __name__ == "__main__":
    raise SystemExit(main())
