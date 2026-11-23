#!/usr/bin/env python3
"""Regression tests for private BIN/CUE staging."""
from pathlib import Path
import hashlib
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts/stage_game_disc.py"


def run(*args):
    return subprocess.run([sys.executable, str(SCRIPT), *map(str, args)],
                          capture_output=True, text=True)


with tempfile.TemporaryDirectory(prefix="cb-stage-test-") as temporary:
    root = Path(temporary)
    source = root / "source"
    destination = root / "pkg" / "USRDIR" / "PSXISO"
    source.mkdir()
    cue = source / "Crash Bash (USA).cue"
    track = source / "Crash Bash (USA).bin"
    cue.write_text('FILE "Crash Bash (USA).bin" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n',
                   encoding="utf-8")
    track.write_bytes(b"\x5a" * 2352 * 3)

    result = run("--source", source, "--destination", destination, "--required")
    assert result.returncode == 0, result.stderr
    assert (destination / cue.name).read_bytes() == cue.read_bytes()
    assert (destination / track.name).read_bytes() == track.read_bytes()
    manifest = (destination / "DISC-MANIFEST.txt").read_text()
    assert hashlib.sha256(track.read_bytes()).hexdigest() in manifest

    archive = root / "disc.zip"
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as bundle:
        bundle.write(cue, cue.name)
        bundle.write(track, track.name)
    archive_destination = root / "archive-pkg"
    result = run("--source", root / "unused", "--destination", archive_destination,
                 "--archive", archive, "--required")
    assert result.returncode == 0, result.stderr
    assert (archive_destination / cue.name).is_file()
    assert (archive_destination / track.name).is_file()

    bad_source = root / "bad-source"
    bad_source.mkdir()
    (bad_source / "bad.cue").write_text('FILE "missing.bin" BINARY\nTRACK 01 MODE2/2352\n')
    result = run("--source", bad_source, "--destination", root / "bad-pkg", "--required")
    assert result.returncode != 0
    assert "missing or empty" in result.stderr

    misaligned = root / "misaligned"
    misaligned.mkdir()
    (misaligned / "bad.cue").write_text('FILE "bad.bin" BINARY\nTRACK 01 MODE2/2352\n')
    (misaligned / "bad.bin").write_bytes(b"x" * 2353)
    result = run("--source", misaligned, "--destination", root / "bad-pkg-2", "--required")
    assert result.returncode != 0
    assert "2352-byte" in result.stderr

    empty = root / "empty"
    empty.mkdir()
    (destination / "stale.cue").write_text("stale")
    result = run("--source", empty, "--destination", destination)
    assert result.returncode == 0
    assert not list(destination.glob("*.cue"))

    unsafe_archive = root / "unsafe.zip"
    with zipfile.ZipFile(unsafe_archive, "w") as bundle:
        bundle.writestr("../escape.bin", b"x")
    result = run("--source", root / "unused", "--destination", root / "unsafe",
                 "--archive", unsafe_archive, "--required")
    assert result.returncode != 0
    assert "unsafe archive member" in result.stderr

print("Personal BIN/CUE staging tests passed.")
