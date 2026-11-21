#!/usr/bin/env python3
"""Read-only parser for PS3 PARAM.SFO metadata.

This tool identifies a user's local game dump by reading the PSF metadata only.
It does not modify files, decrypt SELF containers, or access PSN/account data.
"""
from __future__ import annotations
import argparse, hashlib, json, struct, sys
from pathlib import Path
from typing import Any

PSF_MAGIC = b"\x00PSF"
PSF_HEADER_SIZE = 20
PSF_ENTRY_SIZE = 16
PSF_TYPE_BINARY = 0x0004
PSF_TYPE_STRING = 0x0204
PSF_TYPE_INTEGER = 0x0404
KNOWN_TITLE_IDS = {
    "BLUS31270", "NPUB31301", "NPUB50021", "BLES01945", "BLES01948",
    "NPEB01832", "NPEB01835", "BLJM61125", "BLJM61126", "NPJB00528",
}

class SfoFormatError(ValueError):
    """Raised when a PARAM.SFO file is malformed or unsupported."""

def _range(data: bytes, offset: int, size: int, label: str) -> bytes:
    if offset < 0 or size < 0 or offset > len(data) or size > len(data) - offset:
        raise SfoFormatError(f"{label} points outside the file")
    return data[offset:offset + size]

def parse_sfo(data: bytes) -> dict[str, Any]:
    if len(data) < PSF_HEADER_SIZE:
        raise SfoFormatError("file is shorter than the PSF header")
    if data[:4] != PSF_MAGIC:
        raise SfoFormatError("missing PARAM.SFO magic (00 50 53 46)")
    version, key_table_offset, data_table_offset, count = struct.unpack_from("<IIII", data, 4)
    if count > 4096:
        raise SfoFormatError("entry count exceeds the safety limit")
    entries_start = PSF_HEADER_SIZE
    entries_end = entries_start + count * PSF_ENTRY_SIZE
    _range(data, entries_start, count * PSF_ENTRY_SIZE, "entry table")
    if key_table_offset < entries_end:
        raise SfoFormatError("key table overlaps the header or entry table")
    if data_table_offset < key_table_offset:
        raise SfoFormatError("data table starts before the key table")
    _range(data, key_table_offset, data_table_offset - key_table_offset, "key table")
    _range(data, data_table_offset, len(data) - data_table_offset, "data table")
    entries: dict[str, Any] = {}
    metadata: list[dict[str, Any]] = []
    for index in range(count):
        pos = entries_start + index * PSF_ENTRY_SIZE
        key_offset, fmt, data_len, data_max_len, value_offset = struct.unpack_from("<HHIII", data, pos)
        key_pos = key_table_offset + key_offset
        if key_pos < key_table_offset or key_pos >= data_table_offset:
            raise SfoFormatError(f"entry {index} key offset is outside the key table")
        key_end = data.find(b"\x00", key_pos, data_table_offset)
        if key_end < 0:
            raise SfoFormatError(f"entry {index} key is not NUL-terminated")
        try:
            key = data[key_pos:key_end].decode("ascii")
        except UnicodeDecodeError as exc:
            raise SfoFormatError(f"entry {index} key is not ASCII") from exc
        if not key or key in entries:
            raise SfoFormatError(f"entry {index} has an empty or duplicate key")
        if data_len > data_max_len:
            raise SfoFormatError(f"entry {index} data length exceeds its maximum length")
        raw = _range(data, data_table_offset + value_offset, data_len, f"entry {key} value")
        if fmt == PSF_TYPE_STRING:
            try:
                value: Any = raw.rstrip(b"\x00").decode("utf-8")
            except UnicodeDecodeError as exc:
                raise SfoFormatError(f"entry {key} is not valid UTF-8") from exc
        elif fmt == PSF_TYPE_INTEGER:
            if data_len != 4:
                raise SfoFormatError(f"entry {key} integer is not 4 bytes")
            value = struct.unpack("<I", raw)[0]
        elif fmt == PSF_TYPE_BINARY:
            value = raw.hex()
        else:
            value = {"format": f"0x{fmt:04X}", "hex": raw.hex()}
        entries[key] = value
        metadata.append({
            "key": key, "format": f"0x{fmt:04X}", "data_length": data_len,
            "data_max_length": data_max_len, "data_offset": value_offset,
        })
    title_id = entries.get("TITLE_ID")
    return {
        "format": "PSF", "version": f"0x{version:08X}", "entry_count": count,
        "title_id": title_id,
        "title_id_known_ghosts": title_id in KNOWN_TITLE_IDS if isinstance(title_id, str) else False,
        "selected_target_match": title_id == "BLUS31270",
        "title": entries.get("TITLE"), "app_version": entries.get("APP_VER"),
        "version_string": entries.get("VERSION"), "entries": entries,
        "entry_metadata": metadata,
    }

def inspect_file(path: Path) -> dict[str, Any]:
    data = path.read_bytes()
    report = parse_sfo(data)
    report["input"] = {"name": path.name, "size_bytes": len(data),
                       "sha256": hashlib.sha256(data).hexdigest()}
    report["warning"] = (
        "PARAM.SFO identifies the metadata in this file only. It does not prove which "
        "EBOOT or FastFiles are installed, and APP_VER is not assumed to be the Title Update."
    )
    return report

def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("param_sfo", type=Path, help="path to the user's local PARAM.SFO")
    parser.add_argument("--output-json", type=Path, help="optional JSON report path")
    args = parser.parse_args(argv)
    try:
        report = inspect_file(args.param_sfo)
    except (OSError, SfoFormatError) as exc:
        print(f"PARAM.SFO inspection failed: {exc}", file=sys.stderr)
        return 2
    rendered = json.dumps(report, indent=2, ensure_ascii=False) + "\n"
    if args.output_json:
        args.output_json.write_text(rendered, encoding="utf-8")
    print(rendered, end="")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
