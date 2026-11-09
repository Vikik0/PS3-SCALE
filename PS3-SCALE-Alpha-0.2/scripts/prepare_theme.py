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

    destination_dir = root / "pkgfiles" / "USRDIR"
    if not check_only:
        destination_dir.mkdir(parents=True, exist_ok=True)
        destination = destination_dir / "theme.mp3"
        destination.write_bytes(audio)
        print(f"Prepared {destination.relative_to(root)} ({len(audio):,} bytes)")

    music1_parts = sorted((root / "audio-source").glob("music1.mp3.part-*"))
    music1_source = root / "audio-source" / "music1.mp3"
    if music1_parts:
        music1 = b"".join(part.read_bytes() for part in music1_parts)
    elif music1_source.is_file():
        music1 = music1_source.read_bytes()
    else:
        music1 = b""

    if music1:
        is_mp3 = music1[:3] == b"ID3" or (
            len(music1) > 2 and music1[0] == 0xFF and (music1[1] & 0xE0) == 0xE0
        )
        if not is_mp3:
            raise SystemExit("ERROR: Music 1 source does not have an MP3 signature")
        if not check_only:
            music1_destination = destination_dir / "music1.mp3"
            music1_destination.write_bytes(music1)
            print(f"Prepared {music1_destination.relative_to(root)} ({len(music1):,} bytes)")
        else:
            print(f"PASS: optional Music 1 source present ({len(music1):,} bytes)")
    else:
        print("INFO: optional Music 1 source absent; runtime will fall back to Music 2")

    if check_only:
        print(f"PASS: {len(parts)} source chunks; {len(audio):,} bytes; SHA-256 {digest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
