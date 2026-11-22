#!/usr/bin/env python3
"""Locate and validate a Clockwork ScriptFile in a raw memory/zone dump; build its replacement asset blob."""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
import zlib
from pathlib import Path

DEFAULT_SCRIPT_NAME = "maps/clockwork_interior"
ASSET_PREFIX_SIZE = 24
GSC_WRAPPER_SIZE = 16


class ScriptAssetError(ValueError):
    pass


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def parse_gscbin(data: bytes) -> dict:
    if len(data) < GSC_WRAPPER_SIZE or data[:4] != b"GSC\0":
        raise ScriptAssetError("compiled input is not a GSC Tool GSC\\0 wrapper")
    compressed_len, stack_len, bytecode_len = struct.unpack_from("<III", data, 4)
    expected_len = GSC_WRAPPER_SIZE + compressed_len + bytecode_len
    if len(data) != expected_len:
        raise ScriptAssetError(
            f"GSCBIN length mismatch: header expects {expected_len}, file has {len(data)}"
        )
    compressed_stack = data[GSC_WRAPPER_SIZE:GSC_WRAPPER_SIZE + compressed_len]
    bytecode = data[GSC_WRAPPER_SIZE + compressed_len:]
    try:
        stack = zlib.decompress(compressed_stack)
    except zlib.error as exc:
        raise ScriptAssetError(f"compressed GSC stack is invalid: {exc}") from exc
    if len(stack) != stack_len:
        raise ScriptAssetError(
            f"decompressed stack length mismatch: header={stack_len}, actual={len(stack)}"
        )
    return {
        "compressed_len": compressed_len,
        "stack_len": stack_len,
        "bytecode_len": bytecode_len,
        "compressed_stack": compressed_stack,
        "stack": stack,
        "bytecode": bytecode,
        "wrapper_bytes": len(data),
        "wrapper_sha256": sha256(data),
    }


def build_scriptfile_asset(gscbin: bytes, script_name: str = DEFAULT_SCRIPT_NAME) -> tuple[bytes, dict]:
    try:
        name = script_name.encode("ascii")
    except UnicodeEncodeError as exc:
        raise ScriptAssetError("ScriptFile name must be ASCII") from exc
    if not name or b"\0" in name:
        raise ScriptAssetError("ScriptFile name must be a non-empty ASCII name without NUL")
    parsed = parse_gscbin(gscbin)
    header = (
        b"\xff" * 4
        + struct.pack(">III", parsed["compressed_len"], parsed["stack_len"], parsed["bytecode_len"])
        + b"\xff" * 8
        + name
        + b"\0"
    )
    asset = header + parsed["compressed_stack"] + parsed["bytecode"]
    report = {
        "script_name": script_name,
        "asset_bytes": len(asset),
        "asset_sha256": sha256(asset),
        "header_bytes": len(header),
        "compressed_len": parsed["compressed_len"],
        "stack_len": parsed["stack_len"],
        "bytecode_len": parsed["bytecode_len"],
        "gscbin_bytes": parsed["wrapper_bytes"],
        "gscbin_sha256": parsed["wrapper_sha256"],
    }
    return asset, report


def find_script_assets(
    blob: bytes,
    script_name: str = DEFAULT_SCRIPT_NAME,
    *,
    base_address: int | None = None,
) -> list[dict]:
    try:
        name = script_name.encode("ascii")
    except UnicodeEncodeError as exc:
        raise ScriptAssetError("ScriptFile name must be ASCII") from exc
    needle = name + b"\0"
    results = []
    cursor = 0
    while True:
        name_offset = blob.find(needle, cursor)
        if name_offset < 0:
            break
        cursor = name_offset + 1
        header_offset = name_offset - ASSET_PREFIX_SIZE
        if header_offset < 0:
            continue
        prefix = blob[header_offset:name_offset]
        if len(prefix) != ASSET_PREFIX_SIZE:
            continue
        if prefix[:4] != b"\xff" * 4 or prefix[16:24] != b"\xff" * 8:
            continue
        compressed_len, stack_len, bytecode_len = struct.unpack_from(">III", prefix, 4)
        body_offset = name_offset + len(needle)
        end_offset = body_offset + compressed_len + bytecode_len
        if compressed_len == 0 or stack_len == 0 or bytecode_len == 0 or end_offset > len(blob):
            continue
        compressed_stack = blob[body_offset:body_offset + compressed_len]
        bytecode = blob[body_offset + compressed_len:end_offset]
        try:
            stack = zlib.decompress(compressed_stack)
        except zlib.error:
            continue
        if len(stack) != stack_len:
            continue
        asset = blob[header_offset:end_offset]
        row = {
            "script_name": script_name,
            "header_offset": header_offset,
            "name_offset": name_offset,
            "body_offset": body_offset,
            "end_offset": end_offset,
            "asset_bytes": len(asset),
            "asset_sha256": sha256(asset),
            "compressed_len": compressed_len,
            "stack_len": stack_len,
            "bytecode_len": bytecode_len,
            "compressed_stack_sha256": sha256(compressed_stack),
            "bytecode_sha256": sha256(bytecode),
            "stack_sha256": sha256(stack),
        }
        if base_address is not None:
            row["header_address"] = f"0x{base_address + header_offset:X}"
            row["body_address"] = f"0x{base_address + body_offset:X}"
            row["end_address"] = f"0x{base_address + end_offset:X}"
        results.append(row)
    return results


def analyze(
    *,
    raw_zone: Path | None,
    memory_dump: Path | None,
    gscbin: Path | None,
    script_name: str = DEFAULT_SCRIPT_NAME,
    base_address: int | None = None,
) -> tuple[dict, bytes | None]:
    if raw_zone is None and memory_dump is None and gscbin is None:
        raise ValueError("provide at least one of --raw-zone, --memory-dump, or --gscbin")
    if raw_zone is not None and memory_dump is not None:
        raise ValueError("provide --raw-zone or --memory-dump, not both")

    scan_path = raw_zone or memory_dump
    scan_results = []
    scan_blob = None
    if scan_path is not None:
        scan_blob = Path(scan_path).read_bytes()
        scan_results = find_script_assets(scan_blob, script_name, base_address=base_address)

    replacement_asset = None
    replacement_report = None
    if gscbin is not None:
        replacement_asset, replacement_report = build_scriptfile_asset(
            Path(gscbin).read_bytes(), script_name
        )

    size_comparison = None
    if scan_results and replacement_report:
        original_size = scan_results[0]["asset_bytes"]
        replacement_size = replacement_report["asset_bytes"]
        size_comparison = {
            "original_asset_bytes": original_size,
            "replacement_asset_bytes": replacement_size,
            "delta_bytes": replacement_size - original_size,
            "in_place_replacement_fits": replacement_size <= original_size,
            "runtime_relocation_or_memory_growth_required": replacement_size > original_size,
        }

    if scan_path is not None and not scan_results:
        status = "SCRIPT_ASSET_NOT_FOUND"
    elif scan_results and replacement_report:
        status = "SCRIPT_ASSET_FOUND_REPLACEMENT_BLOB_BUILT"
    elif scan_results:
        status = "SCRIPT_ASSET_FOUND"
    else:
        status = "REPLACEMENT_BLOB_BUILT"

    report = {
        "status": status,
        "script_name": script_name,
        "scan_input": str(scan_path) if scan_path is not None else None,
        "scan_input_bytes": len(scan_blob) if scan_blob is not None else None,
        "scan_input_sha256": sha256(scan_blob) if scan_blob is not None else None,
        "matches": scan_results,
        "replacement_asset": replacement_report,
        "size_comparison": size_comparison,
        "limitations": [
            "This tool only locates and validates a ScriptFile byte sequence in a supplied dump.",
            "It does not locate the engine's ScriptFile registry entry or update any pointer.",
            "It does not write process memory, bypass FastFile authentication, rebuild a FastFile, or create a PKG.",
            "A larger replacement asset requires a verified relocation/allocation strategy; it must not be truncated to fit.",
            "A memory-dump offset is only a virtual address when the correct dump base address is supplied.",
        ],
    }
    return report, replacement_asset


def parse_address(value: str) -> int:
    return int(value, 0)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--raw-zone", type=Path, help="outer-decompressed zone with ScriptFile assets")
    parser.add_argument("--memory-dump", type=Path, help="raw PS3 process memory dump")
    parser.add_argument("--base-address", type=parse_address, help="virtual address represented by byte offset zero")
    parser.add_argument("--gscbin", type=Path, help="compiled GSC Tool wrapper to convert into a ScriptFile asset")
    parser.add_argument("--script-name", default=DEFAULT_SCRIPT_NAME)
    parser.add_argument("--asset-output", type=Path, help="optional output path for the replacement ScriptFile asset blob")
    parser.add_argument("--json-out", type=Path, help="optional JSON report output")
    args = parser.parse_args(argv)

    try:
        report, replacement_asset = analyze(
            raw_zone=args.raw_zone,
            memory_dump=args.memory_dump,
            gscbin=args.gscbin,
            script_name=args.script_name,
            base_address=args.base_address,
        )
        if args.asset_output:
            if replacement_asset is None:
                raise ValueError("--asset-output requires --gscbin")
            args.asset_output.parent.mkdir(parents=True, exist_ok=True)
            args.asset_output.write_bytes(replacement_asset)
            report["replacement_asset"]["output_path"] = str(args.asset_output)
    except (OSError, ValueError, ScriptAssetError, struct.error) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    rendered = json.dumps(report, indent=2, sort_keys=True, ensure_ascii=False) + "\n"
    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(rendered, encoding="utf-8")
    print(rendered, end="")
    return 0 if report["status"] != "SCRIPT_ASSET_NOT_FOUND" else 3


if __name__ == "__main__":
    raise SystemExit(main())
