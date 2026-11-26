#!/usr/bin/env python3
"""Regression tests for safe BIN/CUE staging into a private PS3 PKG."""
from __future__ import annotations

import hashlib
import subprocess
import sys
import tempfile
import warnings
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "stage_game_disc.py"
BIN_NAME = "Crash Bash (USA).bin"
CUE_NAME = "Crash Bash (USA).cue"
CUE_TEXT = f'''FILE "{BIN_NAME}" BINARY
  TRACK 01 MODE2/2352
    INDEX 01 00:00:00
'''


def run_stage(source: Path, destination: Path, *, archive: Path | None = None):
    command = [
        sys.executable, str(SCRIPT),
        "--source", str(source),
        "--destination", str(destination),
        "--required",
    ]
    if archive is not None:
        command.extend(["--archive", str(archive)])
    return subprocess.run(command, text=True, capture_output=True, check=False)


def write_source(folder: Path, *, cue: str = CUE_TEXT, bin_bytes: bytes = b"X" * 2352):
    folder.mkdir(parents=True, exist_ok=True)
    (folder / CUE_NAME).write_text(cue, encoding="utf-8")
    (folder / BIN_NAME).write_bytes(bin_bytes)


def test_valid_source(root: Path):
    source, dest = root / "source", root / "dest"
    write_source(source)
    result = run_stage(source, dest)
    assert result.returncode == 0, result.stderr
    assert (dest / CUE_NAME).read_bytes() == (source / CUE_NAME).read_bytes()
    assert (dest / BIN_NAME).read_bytes() == (source / BIN_NAME).read_bytes()
    manifest = (dest / "DISC-MANIFEST.txt").read_text(encoding="utf-8")
    digest = hashlib.sha256((source / BIN_NAME).read_bytes()).hexdigest()
    assert f"SHA256: {digest}" in manifest


def test_missing_track_fails(root: Path):
    source, dest = root / "missing-source", root / "missing-dest"
    write_source(source, cue=CUE_TEXT.replace(BIN_NAME, "missing.bin"))
    result = run_stage(source, dest)
    assert result.returncode == 2
    assert "missing or empty track" in result.stderr


def test_misaligned_track_fails(root: Path):
    source, dest = root / "bad-sector-source", root / "bad-sector-dest"
    write_source(source, bin_bytes=b"X" * 2353)
    result = run_stage(source, dest)
    assert result.returncode == 2
    assert "not aligned to 2352-byte" in result.stderr


def test_cue_path_traversal_fails(root: Path):
    source, dest = root / "traversal-source", root / "traversal-dest"
    write_source(source, cue=CUE_TEXT.replace(BIN_NAME, "../outside.bin"))
    (root / "outside.bin").write_bytes(b"X" * 2352)
    result = run_stage(source, dest)
    assert result.returncode == 2
    assert "track paths must be filenames" in result.stderr


def write_zip(path: Path, members: list[tuple[str, bytes]]):
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for name, data in members:
            archive.writestr(name, data)


def test_valid_archive(root: Path):
    archive, dest = root / "valid.zip", root / "archive-dest"
    write_zip(archive, [
        (CUE_NAME, CUE_TEXT.encode("utf-8")),
        (BIN_NAME, b"Z" * 2352),
    ])
    result = run_stage(root, dest, archive=archive)
    assert result.returncode == 0, result.stderr
    assert (dest / BIN_NAME).read_bytes() == b"Z" * 2352


def test_unsafe_archive_path_fails(root: Path):
    archive, dest = root / "unsafe.zip", root / "unsafe-dest"
    write_zip(archive, [
        (CUE_NAME, CUE_TEXT.encode("utf-8")),
        ("../escape.bin", b"X" * 2352),
    ])
    result = run_stage(root, dest, archive=archive)
    assert result.returncode == 2
    assert "unsafe archive member path" in result.stderr


def test_duplicate_archive_name_fails(root: Path):
    archive, dest = root / "duplicate.zip", root / "duplicate-dest"
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", UserWarning)
        write_zip(archive, [
            (CUE_NAME, CUE_TEXT.encode("utf-8")),
            (CUE_NAME.upper(), CUE_TEXT.encode("utf-8")),
            (BIN_NAME, b"X" * 2352),
        ])
    result = run_stage(root, dest, archive=archive)
    assert result.returncode == 2
    assert "duplicate filename" in result.stderr


def test_unexpected_archive_member_fails(root: Path):
    archive, dest = root / "extra.zip", root / "extra-dest"
    write_zip(archive, [
        (CUE_NAME, CUE_TEXT.encode("utf-8")),
        (BIN_NAME, b"X" * 2352),
        ("README.txt", b"not a disc track"),
    ])
    result = run_stage(root, dest, archive=archive)
    assert result.returncode == 2
    assert "unexpected file in disc archive" in result.stderr


def main():
    with tempfile.TemporaryDirectory(prefix="cb-disc-tests-") as temporary:
        root = Path(temporary)
        test_valid_source(root)
        test_missing_track_fails(root)
        test_misaligned_track_fails(root)
        test_cue_path_traversal_fails(root)
        test_valid_archive(root)
        test_unsafe_archive_path_fails(root)
        test_duplicate_archive_name_fails(root)
        test_unexpected_archive_member_fails(root)
    print("BIN/CUE staging security tests passed (8 cases).")


if __name__ == "__main__":
    main()
