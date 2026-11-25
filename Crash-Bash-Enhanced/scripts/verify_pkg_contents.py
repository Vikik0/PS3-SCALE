#!/usr/bin/env python3
"""Verify that the generated PS3 PKG contains every staged path before finalization."""

from __future__ import annotations

import re
import struct
import subprocess
import sys
from pathlib import Path

MAX_PKG_ENTRIES = 1024
ENTRY_PATTERN = re.compile(
    r"^\s*(?:NPDRM SELF|directory|raw data|unknown):[+-]\s+\d+:\s+(.+)$"
)


def fail(message: str) -> int:
    print(f"PKG verification failed: {message}", file=sys.stderr)
    return 1


def main() -> int:
    if len(sys.argv) != 4:
        return fail("usage: verify_pkg_contents.py <pkg> <staging-root> <report-path>")

    pkg_path = Path(sys.argv[1])
    staging_root = Path(sys.argv[2])
    report_path = Path(sys.argv[3])

    if not pkg_path.is_file() or not staging_root.is_dir():
        return fail("PKG or staging directory does not exist")

    header = pkg_path.read_bytes()[:0x20]
    if len(header) < 0x20 or header[:4] != b"\x7fPKG":
        return fail("invalid PS3 PKG magic or truncated header")

    package_type = struct.unpack_from(">I", header, 4)[0]
    if package_type != 1:
        return fail(
            f"expected an unfinalized PKG type 0x00000001 for listing, got 0x{package_type:08x}"
        )

    item_count = struct.unpack_from(">I", header, 0x14)[0]
    if item_count <= 0 or item_count >= MAX_PKG_ENTRIES:
        return fail(f"PKG item count {item_count} reaches the PSL1GHT {MAX_PKG_ENTRIES}-entry cap")

    expected_names = set()
    for path in staging_root.rglob("*"):
        relative = path.relative_to(staging_root)
        if any(part.startswith(".") for part in relative.parts):
            continue
        expected_names.add(relative.as_posix())

    if len(expected_names) != item_count:
        return fail(
            f"staging has {len(expected_names)} entries but PKG header declares {item_count}; "
            "the package tool may have truncated the file table"
        )

    listed = subprocess.run(
        ["pkg", "-l", str(pkg_path)],
        check=True,
        capture_output=True,
        text=True,
    )

    actual_names = set()
    for line in listed.stdout.splitlines():
        match = ENTRY_PATTERN.match(line)
        if match:
            name = match.group(1).strip().replace("\\", "/").lstrip("/")
            actual_names.add(name)

    if len(actual_names) != item_count:
        return fail(
            f"pkg -l listed {len(actual_names)} unique entries but the header declares {item_count}"
        )

    missing = sorted(expected_names - actual_names)
    extra = sorted(actual_names - expected_names)
    if missing or extra:
        details = []
        if missing:
            details.append("missing: " + ", ".join(missing[:20]))
        if extra:
            details.append("unexpected: " + ", ".join(extra[:20]))
        return fail("; ".join(details))

    report_path.parent.mkdir(parents=True, exist_ok=True)
    report = [
        f"PKG item count: {item_count} / {MAX_PKG_ENTRIES}",
        f"Staging entries verified: {len(expected_names)}",
        "All package paths:",
        *sorted(actual_names),
    ]
    report_path.write_text("\n".join(report) + "\n", encoding="utf-8")
    print(
        f"Verified complete PS3 PKG: {item_count} entries, "
        f"all {len(expected_names)} staged paths present, below {MAX_PKG_ENTRIES}-entry cap."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
