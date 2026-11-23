#!/usr/bin/env python3
"""Inventory all Ghosts PS3 FastFiles and standalone map assets under a user-owned USRDIR.

This is an offline, read-only pass. It records unsupported containers instead of
silently skipping them, and it never modifies input game files.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import sys
from dataclasses import asdict
from pathlib import Path
from typing import Any

from ps3_fastfile_inspect import FastFileError, inspect_file_full
from ps3_zone_pool_inspect import PoolError, inspect_pool
from ps3_zone_script_export import ZoneScanError, scan_zone
from ps3_zone_mapents_probe import MapEntsError, find_mapents as find_zone_mapents, parse_script_strings
from ps3_mapents_inspect import find_mapents as find_standalone_mapents

MAX_FILES_DEFAULT = 2000
MAX_STANDALONE_MAP_BYTES = 512 * 1024 * 1024
MAP_PATH_RE = re.compile(
    rb"(?i)(?:^|[^A-Za-z0-9_])((?:maps|mapents)/[A-Za-z0-9_./-]{1,180}\.(?:d3dbsp|ents|ff))"
)
INVENTORY_SUFFIXES = {
    ".ff", ".d3dbsp", ".ents", ".zone", ".gsc", ".csc", ".gscbin", ".cscbin"
}


class UsrdirScanError(ValueError):
    """Raised when the requested USRDIR inventory cannot be created safely."""


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def find_map_path_references(data: bytes) -> list[str]:
    found: set[str] = set()
    for match in MAP_PATH_RE.finditer(data):
        try:
            value = match.group(1).decode("ascii")
        except UnicodeDecodeError:
            continue
        found.add(value.replace("\\", "/"))
    return sorted(found, key=str.casefold)


def _scan_fastfile(path: Path, root: Path) -> dict[str, Any]:
    relative = path.relative_to(root).as_posix()
    base: dict[str, Any] = {
        "path": relative,
        "name": path.name,
        "size_bytes": path.stat().st_size,
        "sha256": sha256_file(path),
        "container": "IW6_PS3_FASTFILE",
    }
    try:
        report, inflated_zone, raw_zone = inspect_file_full(path)
    except (FastFileError, OSError, ValueError) as exc:
        return {
            **base,
            "status": "UNSUPPORTED_OR_INVALID_FASTFILE",
            "error": str(exc),
        }

    base.update({
        "status": "VALIDATED_COMPLETE" if report.complete else "VALIDATED_PARTIAL_STREAM",
        "version": f"0x{report.version:08X}",
        "inner_magic_offset": f"0x{report.inner_magic_offset:X}",
        "stream_offset": f"0x{report.stream_offset:X}",
        "block_count": report.block_count,
        "block_trailer_size": report.block_trailer_size,
        "raw_zone_size": report.outer_zone_size,
        "inflated_zone_size": report.decompressed_size,
        "inner_streams_inflated": report.inner_streams_inflated,
        "trailing_bytes": report.trailing_bytes,
        "trailing_sha256": report.trailing_sha256,
        "raw_zone_sha256": report.raw_zone_sha256,
        "inflated_zone_sha256": report.zone_sha256,
        "map_path_references": find_map_path_references(inflated_zone),
    })

    try:
        pool = inspect_pool(raw_zone)
        base["asset_pool"] = {
            "asset_count_header": pool["asset_count_header"],
            "valid_record_count": pool["valid_record_count"],
            "type_counts": pool["type_counts"],
            "scriptfile_count": pool["scriptfile_count_type_0x29"],
            "rawfile_count": pool["rawfile_count_type_0x28"],
            "map_ents_count": pool["map_ents_count_type_0x14"],
            "map_ents_entries": pool["map_ents_entries"],
            "pool_offset": pool["pool_offset"],
            "pool_trailer_bytes": pool["pool_trailer_bytes"],
        }
    except (PoolError, ValueError, struct.error) as exc:
        base["asset_pool"] = {"status": "UNAVAILABLE", "error": str(exc)}

    try:
        assets = scan_zone(raw_zone)
        base["script_assets"] = [
            {
                "name": record.name,
                "kind": record.kind,
                "sha256": record.sha256,
                "output_size": record.output_size,
            }
            for record, _payload in assets
            if record.kind in ("compiled_gsc_candidate", "gsc_source")
        ]
        base["asset_scan"] = {
            "status": "COMPLETED",
            "asset_count": len(assets),
            "compiled_gsc_candidates": sum(
                record.kind == "compiled_gsc_candidate" for record, _ in assets
            ),
            "gsc_source_count": sum(record.kind == "gsc_source" for record, _ in assets),
            "rawfile_count": sum(record.kind == "rawfile" for record, _ in assets),
        }
    except (ZoneScanError, ValueError, struct.error) as exc:
        base["asset_scan"] = {"status": "UNAVAILABLE", "error": str(exc)}
        base["script_assets"] = []

    try:
        script_strings = parse_script_strings(inflated_zone)
        candidates = find_zone_mapents(inflated_zone, script_strings)
        base["mapents_scan"] = {
            "status": "COMPLETED",
            "script_string_count": script_strings["count"],
            "candidate_count": len(candidates),
            "candidates": candidates,
        }
    except (MapEntsError, ValueError, IndexError, struct.error) as exc:
        base["mapents_scan"] = {
            "status": "UNAVAILABLE",
            "candidate_count": 0,
            "error": str(exc),
        }

    return base


def _scan_standalone_map(path: Path, root: Path) -> dict[str, Any]:
    relative = path.relative_to(root).as_posix()
    size = path.stat().st_size
    row: dict[str, Any] = {
        "path": relative,
        "name": path.name,
        "size_bytes": size,
        "container": "STANDALONE_MAP_OR_ENTITY_FILE",
    }
    if size > MAX_STANDALONE_MAP_BYTES:
        return {
            **row,
            "status": "SKIPPED_SIZE_LIMIT",
            "candidate_count": 0,
        }
    try:
        data = path.read_bytes()
        candidates = find_standalone_mapents(data)
        return {
            **row,
            "status": "SCANNED",
            "sha256": hashlib.sha256(data).hexdigest(),
            "candidate_count": len(candidates),
            "candidates": [asdict(item[0]) for item in candidates],
        }
    except (OSError, ValueError, IndexError, struct.error) as exc:
        return {
            **row,
            "status": "SCAN_FAILED",
            "candidate_count": 0,
            "error": str(exc),
        }


def scan_usrdir(root: Path, *, max_files: int = MAX_FILES_DEFAULT) -> dict[str, Any]:
    root = Path(root).expanduser().resolve()
    if not root.is_dir():
        raise UsrdirScanError(f"USRDIR path is not a directory: {root}")
    if max_files < 1:
        raise UsrdirScanError("--max-files must be positive")

    candidates = sorted(
        (
            path for path in root.rglob("*")
            if path.is_file() and path.suffix.lower() in INVENTORY_SUFFIXES
        ),
        key=lambda path: path.relative_to(root).as_posix().casefold(),
    )
    if len(candidates) > max_files:
        raise UsrdirScanError(
            f"found {len(candidates)} candidate files, above --max-files={max_files}; "
            "raise the limit explicitly after reviewing the directory"
        )

    fastfiles = []
    standalone_maps = []
    other_files = []
    for path in candidates:
        suffix = path.suffix.lower()
        if suffix == ".ff":
            fastfiles.append(_scan_fastfile(path, root))
        elif suffix in (".d3dbsp", ".ents"):
            standalone_maps.append(_scan_standalone_map(path, root))
        else:
            other_files.append({
                "path": path.relative_to(root).as_posix(),
                "name": path.name,
                "size_bytes": path.stat().st_size,
                "extension": suffix,
            })

    supported = sum(
        row.get("status") in ("VALIDATED_COMPLETE", "VALIDATED_PARTIAL_STREAM")
        for row in fastfiles
    )
    mapents_candidates = sum(
        row.get("mapents_scan", {}).get("candidate_count", 0)
        for row in fastfiles
    ) + sum(row.get("candidate_count", 0) for row in standalone_maps)

    return {
        "schema_version": 1,
        "status": "USRDIR_INVENTORY_COMPLETED",
        "root": str(root),
        "candidate_file_count": len(candidates),
        "fastfile_count": len(fastfiles),
        "fastfiles_parsed": supported,
        "fastfiles_unsupported_or_invalid": len(fastfiles) - supported,
        "standalone_map_or_entity_file_count": len(standalone_maps),
        "standalone_map_files_scanned": sum(row["status"] == "SCANNED" for row in standalone_maps),
        "mapents_candidate_count": mapents_candidates,
        "script_asset_count": sum(
            row.get("asset_scan", {}).get("compiled_gsc_candidates", 0)
            + row.get("asset_scan", {}).get("gsc_source_count", 0)
            for row in fastfiles
        ),
        "fastfiles": fastfiles,
        "standalone_maps": standalone_maps,
        "other_candidate_files": other_files,
        "interpretation": (
            "This is an offline inventory of the supplied directory. A MapEnts candidate "
            "is not accepted as a spawn table until its structure and script-string indices "
            "validate. Unsupported FastFile containers are listed, not silently ignored. "
            "No PS3 runtime state or second Campaign local client is inferred."
        ),
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("usrdir", type=Path, help="extracted BLUS31270 PS3_GAME/USRDIR directory")
    parser.add_argument("--output-json", type=Path, required=True, help="write the complete inventory JSON")
    parser.add_argument("--max-files", type=int, default=MAX_FILES_DEFAULT)
    args = parser.parse_args(argv)
    try:
        report = scan_usrdir(args.usrdir, max_files=args.max_files)
        rendered = json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True) + "\n"
        args.output_json.parent.mkdir(parents=True, exist_ok=True)
        args.output_json.write_text(rendered, encoding="utf-8")
        print(rendered, end="")
        return 0
    except (OSError, UsrdirScanError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
