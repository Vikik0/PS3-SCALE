#!/usr/bin/env python3
"""Validate and stage a user-owned Crash Bash BIN/CUE pair for a private PS3 PKG."""
from __future__ import annotations

import argparse
import hashlib
import re
import shutil
import sys
import tempfile
import zipfile
from pathlib import Path, PurePosixPath

FILE_RE = re.compile(r'^\s*FILE\s+(?:"([^"]+)"|\'([^\']+)\'|(\S+))\s+(\S+)\s*$', re.I)
TRACK_RE = re.compile(r'^\s*TRACK\s+\d+\s+(\S+)\s*
MAX_ARCHIVE_BYTES = 512 * 1024 * 1024


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def read_cue(path: Path) -> tuple[list[tuple[str, str]], list[str]]:
    try:
        text = path.read_text(encoding="utf-8-sig")
    except UnicodeDecodeError:
        text = path.read_text(encoding="cp1252")
    files: list[tuple[str, str]] = []
    track_modes: list[str] = []
    for line_no, line in enumerate(text.splitlines(), 1):
        track = TRACK_RE.match(line)
        if track:
            track_modes.append(track.group(1).upper())
        match = FILE_RE.match(line)
        if not match:
            continue
        filename = next(value for value in match.groups()[:3] if value is not None)
        file_type = match.group(4).upper()
        if not filename or filename in {".", ".."}:
            raise ValueError(f"CUE line {line_no}: invalid FILE name")
        if "/" in filename or "\\" in filename or Path(filename).is_absolute():
            raise ValueError(f"CUE line {line_no}: track paths must be filenames beside the CUE")
        if file_type != "BINARY":
            raise ValueError(f"CUE line {line_no}: only BINARY tracks are supported for this PS1 package")
        files.append((filename, file_type))
    if not files:
        raise ValueError("CUE contains no FILE ... BINARY track entries")
    if not track_modes:
        raise ValueError("CUE contains no TRACK entries")
    return files, track_modes


def safe_extract_archive(archive: Path, destination: Path) -> None:
    destination.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive) as bundle:
        members = [item for item in bundle.infolist() if not item.is_dir()]
        total = sum(item.file_size for item in members)
        if total > MAX_ARCHIVE_BYTES:
            raise ValueError("disc archive expands beyond the 512 MiB safety limit")
        if not members:
            raise ValueError("disc archive is empty")
        seen: set[str] = set()
        for item in members:
            name = item.filename
            posix = PurePosixPath(name)
            if posix.is_absolute() or len(posix.parts) != 1 or posix.name in {"", ".", ".."} or "\\" in name:
                raise ValueError(f"unsafe archive member path: {name!r}")
            suffix = Path(name).suffix.lower()
            if suffix not in {".cue", ".bin"}:
                raise ValueError(f"unexpected file in disc archive: {name!r}")
            if name.casefold() in seen:
                raise ValueError(f"duplicate filename in disc archive: {name!r}")
            seen.add(name.casefold())
            target = destination / name
            with bundle.open(item) as source, target.open("wb") as output:
                shutil.copyfileobj(source, output, length=1024 * 1024)


def stage(source: Path, destination: Path, required: bool) -> int:
    cues = sorted((p for p in source.iterdir() if p.is_file() and p.suffix.lower() == ".cue"),
                  key=lambda p: p.name.casefold()) if source.is_dir() else []
    if len(cues) != 1:
        if required:
            raise ValueError(f"expected exactly one CUE file in {source}, found {len(cues)}")
        if destination.exists():
            shutil.rmtree(destination)
        destination.mkdir(parents=True, exist_ok=True)
        print("No user disc supplied; this package build will require an external PSXISO dump.")
        return 0

    cue = cues[0]
    file_entries, track_modes = read_cue(cue)
    referenced: list[Path] = []
    for filename, _ in file_entries:
        track = source / filename
        if not track.is_file() or track.stat().st_size <= 0:
            raise ValueError(f"CUE references a missing or empty track file: {filename}")
        if track not in referenced:
            referenced.append(track)

    mode_2352 = any("2352" in mode for mode in track_modes)
    mode_2048 = any("2048" in mode for mode in track_modes)
    for track in referenced:
        size = track.stat().st_size
        if mode_2352 and size % 2352 != 0:
            raise ValueError(f"{track.name}: size is not aligned to 2352-byte CD sectors")
        if not mode_2352 and mode_2048 and size % 2048 != 0:
            raise ValueError(f"{track.name}: size is not aligned to 2048-byte sectors")

    if destination.exists():
        shutil.rmtree(destination)
    destination.mkdir(parents=True, exist_ok=True)
    shutil.copy2(cue, destination / cue.name)
    for track in referenced:
        shutil.copy2(track, destination / track.name)

    manifest = [
        "Crash Bash personal disc payload",
        f"CUE: {cue.name}",
        f"CUE SHA256: {sha256(cue)}",
    ]
    for track in referenced:
        manifest.extend([
            f"TRACK: {track.name}",
            f"SIZE: {track.stat().st_size}",
            f"SHA256: {sha256(track)}",
        ])
    (destination / "DISC-MANIFEST.txt").write_text("\n".join(manifest) + "\n", encoding="utf-8")
    print(f"Embedded disc staged: {1 + len(referenced)} source file(s), "
          f"{cue.stat().st_size + sum(p.stat().st_size for p in referenced):,} bytes.")
    print(f"Installed path: {destination}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--destination", type=Path, required=True)
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--required", action="store_true",
                        help="fail instead of producing an external-disc-only package")
    args = parser.parse_args()
    try:
        if args.archive:
            with tempfile.TemporaryDirectory(prefix="cb-disc-") as temporary:
                source = Path(temporary) / "source"
                safe_extract_archive(args.archive, source)
                return stage(source, args.destination, args.required)
        return stage(args.source, args.destination, args.required)
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        print(f"Disc staging failed: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
, re.I)
MAX_ARCHIVE_BYTES = 512 * 1024 * 1024


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def read_cue(path: Path) -> tuple[list[tuple[str, str]], list[str]]:
    try:
        text = path.read_text(encoding="utf-8-sig")
    except UnicodeDecodeError:
        text = path.read_text(encoding="cp1252")
    files: list[tuple[str, str]] = []
    track_modes: list[str] = []
    for line_no, line in enumerate(text.splitlines(), 1):
        track = TRACK_RE.match(line)
        if track:
            track_modes.append(track.group(1).upper())
        match = FILE_RE.match(line)
        if not match:
            continue
        filename = next(value for value in match.groups()[:3] if value is not None)
        file_type = match.group(4).upper()
        if not filename or filename in {".", ".."}:
            raise ValueError(f"CUE line {line_no}: invalid FILE name")
        if "/" in filename or "\\" in filename or Path(filename).is_absolute():
            raise ValueError(f"CUE line {line_no}: track paths must be filenames beside the CUE")
        if file_type != "BINARY":
            raise ValueError(f"CUE line {line_no}: only BINARY tracks are supported for this PS1 package")
        files.append((filename, file_type))
    if not files:
        raise ValueError("CUE contains no FILE ... BINARY track entries")
    if not track_modes:
        raise ValueError("CUE contains no TRACK entries")
    return files, track_modes


def safe_extract_archive(archive: Path, destination: Path) -> None:
    destination.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive) as bundle:
        members = [item for item in bundle.infolist() if not item.is_dir()]
        total = sum(item.file_size for item in members)
        if total > MAX_ARCHIVE_BYTES:
            raise ValueError("disc archive expands beyond the 512 MiB safety limit")
        if not members:
            raise ValueError("disc archive is empty")
        seen: set[str] = set()
        for item in members:
            name = item.filename
            posix = PurePosixPath(name)
            if posix.is_absolute() or len(posix.parts) != 1 or posix.name in {"", ".", ".."}:
                raise ValueError(f"unsafe archive member path: {name!r}")
            suffix = Path(name).suffix.lower()
            if suffix not in {".cue", ".bin"}:
                raise ValueError(f"unexpected file in disc archive: {name!r}")
            if name.casefold() in seen:
                raise ValueError(f"duplicate filename in disc archive: {name!r}")
            seen.add(name.casefold())
            target = destination / name
            with bundle.open(item) as source, target.open("wb") as output:
                shutil.copyfileobj(source, output, length=1024 * 1024)


def stage(source: Path, destination: Path, required: bool) -> int:
    cues = sorted((p for p in source.iterdir() if p.is_file() and p.suffix.lower() == ".cue"),
                  key=lambda p: p.name.casefold()) if source.is_dir() else []
    if len(cues) != 1:
        if required:
            raise ValueError(f"expected exactly one CUE file in {source}, found {len(cues)}")
        if destination.exists():
            shutil.rmtree(destination)
        destination.mkdir(parents=True, exist_ok=True)
        print("No user disc supplied; this package build will require an external PSXISO dump.")
        return 0

    cue = cues[0]
    file_entries, track_modes = read_cue(cue)
    referenced: list[Path] = []
    for filename, _ in file_entries:
        track = source / filename
        if not track.is_file() or track.stat().st_size <= 0:
            raise ValueError(f"CUE references a missing or empty track file: {filename}")
        if track not in referenced:
            referenced.append(track)

    mode_2352 = any("2352" in mode for mode in track_modes)
    mode_2048 = any("2048" in mode for mode in track_modes)
    for track in referenced:
        size = track.stat().st_size
        if mode_2352 and size % 2352 != 0:
            raise ValueError(f"{track.name}: size is not aligned to 2352-byte CD sectors")
        if not mode_2352 and mode_2048 and size % 2048 != 0:
            raise ValueError(f"{track.name}: size is not aligned to 2048-byte sectors")

    if destination.exists():
        shutil.rmtree(destination)
    destination.mkdir(parents=True, exist_ok=True)
    shutil.copy2(cue, destination / cue.name)
    for track in referenced:
        shutil.copy2(track, destination / track.name)

    manifest = [
        "Crash Bash personal disc payload",
        f"CUE: {cue.name}",
        f"CUE SHA256: {sha256(cue)}",
    ]
    for track in referenced:
        manifest.extend([
            f"TRACK: {track.name}",
            f"SIZE: {track.stat().st_size}",
            f"SHA256: {sha256(track)}",
        ])
    (destination / "DISC-MANIFEST.txt").write_text("\n".join(manifest) + "\n", encoding="utf-8")
    print(f"Embedded disc staged: {1 + len(referenced)} source file(s), "
          f"{cue.stat().st_size + sum(p.stat().st_size for p in referenced):,} bytes.")
    print(f"Installed path: {destination}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--destination", type=Path, required=True)
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--required", action="store_true",
                        help="fail instead of producing an external-disc-only package")
    args = parser.parse_args()
    try:
        if args.archive:
            with tempfile.TemporaryDirectory(prefix="cb-disc-") as temporary:
                source = Path(temporary) / "source"
                safe_extract_archive(args.archive, source)
                return stage(source, args.destination, args.required)
        return stage(args.source, args.destination, args.required)
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        print(f"Disc staging failed: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
