#!/usr/bin/env python3
"""Inventory/export Ghosts PS3 GSC assets from an already inflated IW6 zone.

Input must be the .zone output of a Ghosts-aware FastFile decompressor that
expands both outer raw-deflate blocks and inner per-asset zlib streams. This
script is read-only with respect to the source zone and exports only validated
GSC/CSC candidates. It does not patch or rebuild game files.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from dataclasses import asdict, dataclass
from pathlib import Path, PurePosixPath
from typing import Any

FF4 = b"\xff" * 4
FF8 = b"\xff" * 8
MAX_COMPRESSED_LEN = 0x4000000
MAX_DECOMPRESSED_LEN = 0x4000000
MAX_NAME_LEN = 127
SCRIPT_EXTENSIONS = {".gsc", ".csc", ".gscbin", ".cscbin"}


class ZoneScanError(ValueError):
    """Raised when a zone cannot be scanned safely."""


@dataclass(frozen=True)
class ScriptAsset:
    header_offset: int
    body_offset: int
    body_size: int
    compressed_size_field: int
    header_shape: str
    name: str
    kind: str
    sha256: str
    output_name: str


def _read_name(data: bytes, start: int) -> tuple[str, int] | None:
    end_limit = min(len(data), start + MAX_NAME_LEN + 1)
    end = data.find(b"\x00", start, end_limit)
    if end <= start:
        return None
    raw = data[start:end]
    if any(byte < 0x20 or byte > 0x7E for byte in raw):
        return None
    try:
        name = raw.decode("ascii")
    except UnicodeDecodeError:
        return None
    normalized = name.replace("\\", "/")
    if not name or normalized.startswith("/") or ".." in PurePosixPath(normalized).parts:
        return None
    if re.match(r"^[A-Za-z]:", name):
        return None
    return name, end


def _parse_header(data: bytes, offset: int) -> tuple[str, int, int, str, int] | None:
    if offset < 0 or offset + 16 > len(data) or data[offset:offset + 4] != FF4:
        return None

    comp_len = int.from_bytes(data[offset + 4:offset + 8], "big")
    dec_len = int.from_bytes(data[offset + 8:offset + 12], "big")
    if not (0 < comp_len < MAX_COMPRESSED_LEN and 0 < dec_len < MAX_DECOMPRESSED_LEN):
        return None

    # Long shape is more specific: [FF*4][comp][decomp][unknown][FF*8]<name>\0<body>.
    if offset + 28 <= len(data) and data[offset + 16:offset + 24] == FF8:
        parsed = _read_name(data, offset + 24)
        if parsed:
            name, name_end = parsed
            body_start = name_end + 1
            if body_start + dec_len <= len(data):
                return "long", comp_len, dec_len, name, body_start

    # Short shape: [FF*4][comp][decomp][FF*4]<name>\0<body>.
    if data[offset + 12:offset + 16] == FF4:
        parsed = _read_name(data, offset + 16)
        if parsed:
            name, name_end = parsed
            body_start = name_end + 1
            if body_start + dec_len <= len(data):
                return "short", comp_len, dec_len, name, body_start
    return None


def scan_zone(data: bytes) -> list[tuple[ScriptAsset, bytes]]:
    if len(data) < 0x40:
        raise ZoneScanError("zone is too small to contain an IW6 XFile header")

    found: list[tuple[ScriptAsset, bytes]] = []
    offset = 0x40
    while offset <= len(data) - 16:
        if data[offset:offset + 4] != FF4:
            offset += 1
            continue

        parsed = _parse_header(data, offset)
        if parsed is None:
            offset += 1
            continue

        shape, comp_len, dec_len, name, body_start = parsed
        body = data[body_start:body_start + dec_len]
        suffix = Path(name).suffix.lower()
        is_gsc_magic = body.startswith(b"GSC\x00")
        if suffix in SCRIPT_EXTENSIONS or is_gsc_magic:
            if is_gsc_magic:
                kind = "compiled_gsc"
                if suffix in {".gscbin", ".cscbin"}:
                    output_name = name
                elif suffix == ".csc":
                    output_name = str(Path(name).with_suffix(".cscbin"))
                else:
                    output_name = str(Path(name).with_suffix(".gscbin")) if suffix == ".gsc" else name + ".gscbin"
            else:
                kind = "gsc_source" if suffix in {".gsc", ".csc"} else "script_asset"
                output_name = name
            asset = ScriptAsset(
                header_offset=offset,
                body_offset=body_start,
                body_size=dec_len,
                compressed_size_field=comp_len,
                header_shape=shape,
                name=name,
                kind=kind,
                sha256=hashlib.sha256(body).hexdigest(),
                output_name=output_name,
            )
            found.append((asset, body))

        # Skip this validated body to avoid interpreting embedded binary content
        # as another top-level header. Nested mptype MapEnts blobs are not parsed.
        offset = body_start + dec_len

    return found


def _safe_output_path(root: Path, name: str, used: set[str], offset: int) -> Path:
    normalized = name.replace("\\", "/")
    parts = [part for part in PurePosixPath(normalized).parts if part not in ("", ".")]
    if not parts or any(part == ".." for part in parts):
        raise ZoneScanError(f"unsafe asset path rejected: {name!r}")
    relative = Path(*parts)
    target = root / relative
    try:
        target.resolve().relative_to(root.resolve())
    except ValueError as exc:
        raise ZoneScanError(f"asset path escapes output directory: {name!r}") from exc

    key = str(relative).lower()
    if key in used or target.exists():
        target = target.with_name(f"{target.stem}_{offset:08X}{target.suffix}")
        key = str(target.relative_to(root)).lower()
    used.add(key)
    return target


def export_zone(zone_path: Path, output_dir: Path) -> dict[str, Any]:
    try:
        data = zone_path.read_bytes()
    except OSError as exc:
        raise ZoneScanError(f"cannot read zone: {exc}") from exc
    assets = scan_zone(data)
    output_dir.mkdir(parents=True, exist_ok=True)
    used: set[str] = set()
    entries: list[dict[str, Any]] = []
    for asset, body in assets:
        target = _safe_output_path(output_dir, asset.output_name, used, asset.header_offset)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(body)
        row = asdict(asset)
        row["exported_path"] = str(target)
        entries.append(row)
    return {
        "zone": str(zone_path),
        "zone_size": len(data),
        "script_asset_count": len(entries),
        "assets": entries,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("zone", type=Path, help="already inflated Ghosts PS3 .zone file")
    parser.add_argument("output_dir", type=Path, help="directory for validated GSC/CSC assets")
    parser.add_argument("--report-json", type=Path, help="write the inventory as JSON")
    args = parser.parse_args(argv)

    try:
        report = export_zone(args.zone, args.output_dir)
        rendered = json.dumps(report, indent=2, sort_keys=True)
        print(rendered)
        if args.report_json:
            args.report_json.parent.mkdir(parents=True, exist_ok=True)
            args.report_json.write_text(rendered + "\n", encoding="utf-8")
        return 0
    except (ZoneScanError, OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
