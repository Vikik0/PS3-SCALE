#!/usr/bin/env python3
"""Rebuild and verify the embedded startup theme from ordered binary chunks."""
from __future__ import annotations

import hashlib
import sys
from pathlib import Path

EXPECTED_SIZE = 5_182_732
EXPECTED_SHA256 = "b8a7b1c2573ea8a397c2fbc3a6c731b25dcb71f572aedf5af8568785620d1b64"


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    check_only = "--check-only" in sys.argv[1:]
    parts = sorted((root / "audio-source").glob("theme.mp3.part-*"))
    if not parts:
        raise SystemExit("ERROR: no audio-source/theme.mp3.part-* chunks found")

    audio = b"".join(part.read_bytes() for part in parts)
    digest = hashlib.sha256(audio).hexdigest()
    if len(audio) != EXPECTED_SIZE:
        raise SystemExit(f"ERROR: theme size mismatch: {len(audio)} bytes")
    if digest != EXPECTED_SHA256:
        raise SystemExit(f"ERROR: theme SHA-256 mismatch: {digest}")

    if not check_only:
        destination = root / "pkgfiles" / "USRDIR" / "theme.mp3"
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(audio)
        print(f"Prepared {destination.relative_to(root)} ({len(audio):,} bytes)")
    else:
        print(f"PASS: {len(parts)} source chunks; {len(audio):,} bytes; SHA-256 {digest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
