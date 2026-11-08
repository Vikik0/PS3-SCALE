#!/usr/bin/env python3
"""Verify the embedded MP3 source chunks without changing the working tree."""
from __future__ import annotations

import hashlib
import sys
from pathlib import Path

EXPECTED_SIZE = 5_182_732
EXPECTED_SHA256 = "b8a7b1c2573ea8a397c2fbc3a6c731b25dcb71f572aedf5af8568785620d1b64"

root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]
parts = sorted((root / "audio-source").glob("theme.mp3.part-*"))
assert len(parts) == 14, f"expected 14 ordered MP3 chunks, got {len(parts)}"
audio = b"".join(part.read_bytes() for part in parts)
assert len(audio) == EXPECTED_SIZE, f"unexpected theme size: {len(audio)}"
assert hashlib.sha256(audio).hexdigest() == EXPECTED_SHA256, "theme SHA-256 mismatch"
print("Embedded theme source tests: PASS")
