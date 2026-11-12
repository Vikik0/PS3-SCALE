#!/usr/bin/env python3
"""Verify the embedded MP3 source chunks without changing the working tree."""
from __future__ import annotations

import hashlib
import sys
from pathlib import Path

EXPECTED_SIZE = 5_182_732
EXPECTED_SHA256 = "b8a7b1c2573ea8a397c2fbc3a6c731b25dcb71f572aedf5af8568785620d1b64"
MUSIC1_SIZE = 3_129_868
MUSIC1_SHA256 = "8b567be77af069f8c0010d54f6046b4ae339bf4599d2667939aa45d1d746cf23"

root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]
parts = sorted((root / "audio-source").glob("theme.mp3.part-*"))
assert len(parts) == 14, f"expected 14 ordered MP3 chunks, got {len(parts)}"
audio = b"".join(part.read_bytes() for part in parts)
assert len(audio) == EXPECTED_SIZE, f"unexpected theme size: {len(audio)}"
assert hashlib.sha256(audio).hexdigest() == EXPECTED_SHA256, "theme SHA-256 mismatch"
music1_parts = sorted((root / "audio-source").glob("music1.mp3.part-*"))
music1_source = root / "audio-source" / "music1.mp3"
if music1_parts:
    music1 = b"".join(part.read_bytes() for part in music1_parts)
elif music1_source.is_file():
    music1 = music1_source.read_bytes()
else:
    music1 = b""

if music1:
    assert len(music1) == MUSIC1_SIZE, f"unexpected Music 1 size: {len(music1)}"
    assert hashlib.sha256(music1).hexdigest() == MUSIC1_SHA256, "Music 1 SHA-256 mismatch"
    print("Music 1 source tests: PASS")
else:
    print("Music 1 source tests: NOT PRESENT (USB fallback remains enabled)")

print("Embedded theme source tests: PASS")
