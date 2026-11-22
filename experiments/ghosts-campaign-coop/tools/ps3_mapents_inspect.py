#!/usr/bin/env python3
"""Locate and export an IW6 PS3 MapEnts entity string from a big-endian map .d3dbsp."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import sys
from dataclasses import asdict, dataclass
from pathlib import Path

MAP_ENTS_HEADER_SIZE = 0x80
ENTITY_STRING_POINTER_OFFSET = 0x04
NUM_ENTITY_CHARS_OFFSET = 0x08
SPAWN_COUNT_OFFSET = 0x70
INLINE_POINTER = 0xFFFFFFFF
MAX_ENTITY_CHARS = 8 * 1024 * 1024
MAX_SPAWN_COUNT = 8192
ENTITY_BLOCK_RE = re.compile(rb"\{\s*\n([^{}]*)\}", re.DOTALL)
FIELD_RE = re.compile(rb'(?m)^(\d+) "([^"\r\n]*)"\s*$')


@dataclass
class MapEntsCandidate:
    header_offset: int
    entity_string_offset: int
    entity_chars_including_nul: int
    spawn_count: int
    entity_count: int
    has_classname: bool
    has_targetname: bool
    entity_string_sha256: str
    preview: str


def inspect_candidate(data: bytes, header_offset: int):
    if header_offset < 0 or header_offset + MAP_ENTS_HEADER_SIZE > len(data):
        return None
    pointer = struct.unpack_from(">I", data, header_offset + ENTITY_STRING_POINTER_OFFSET)[0]
    if pointer != INLINE_POINTER:
        return None
    entity_chars = struct.unpack_from(">i", data, header_offset + NUM_ENTITY_CHARS_OFFSET)[0]
    if entity_chars < 4 or entity_chars > MAX_ENTITY_CHARS:
        return None
    entity_start = header_offset + MAP_ENTS_HEADER_SIZE
    entity_end = entity_start + entity_chars
    if entity_end > len(data):
        return None
    raw = data[entity_start:entity_end]
    if raw[-1] != 0 or b"\0" in raw[:-1]:
        return None
    entity_text = raw[:-1]
    stripped = entity_text.strip()
    if not stripped.startswith(b"{") or not stripped.endswith(b"}"):
        return None
    try:
        entity_text.decode("ascii", errors="strict")
    except UnicodeDecodeError:
        return None
    entities = []
    for block in ENTITY_BLOCK_RE.finditer(entity_text):
        fields = {}
        for match in FIELD_RE.finditer(block.group(1)):
            fields[match.group(1).decode("ascii")] = match.group(2).decode("ascii")
        if fields:
            entities.append(fields)
    if not entities or not any("97" in row for row in entities):
        return None
    spawn_count = struct.unpack_from(">H", data, header_offset + SPAWN_COUNT_OFFSET)[0]
    if spawn_count > MAX_SPAWN_COUNT:
        return None
    candidate = MapEntsCandidate(
        header_offset=header_offset,
        entity_string_offset=entity_start,
        entity_chars_including_nul=entity_chars,
        spawn_count=spawn_count,
        entity_count=len(entities),
        has_classname=any("97" in row for row in entities),
        has_targetname=any("642" in row for row in entities),
        entity_string_sha256=hashlib.sha256(raw).hexdigest(),
        preview=entity_text[:180].decode("ascii", errors="replace"),
    )
    return candidate, entity_text, entities


def find_mapents(data: bytes, offset: int | None = None):
    if offset is not None:
        candidate = inspect_candidate(data, offset)
        return [candidate] if candidate else []
    results = []
    needle = struct.pack(">I", INLINE_POINTER)
    pos = 0
    while True:
        pos = data.find(needle, pos)
        if pos < 0:
            break
        candidate = inspect_candidate(data, pos - ENTITY_STRING_POINTER_OFFSET)
        if candidate:
            results.append(candidate)
        pos += 1
    return results


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("map_file", type=Path, help="PS3 IW6 map .d3dbsp or extracted map zone")
    parser.add_argument("--map-ents-offset", type=lambda x: int(x, 0), help="known MapEnts header offset (hex or decimal)")
    parser.add_argument("--output-ents", type=Path, help="write extracted entity text without the final NUL")
    parser.add_argument("--output-json", type=Path, help="write candidate metadata and parsed entity fields")
    args = parser.parse_args(argv)
    try:
        data = args.map_file.read_bytes()
        candidates = find_mapents(data, args.map_ents_offset)
        if not candidates:
            raise ValueError("no valid PS3 IW6 MapEnts entity-string header was found")
        if len(candidates) != 1:
            details = [asdict(item[0]) for item in candidates]
            raise ValueError(
                f"found {len(candidates)} plausible MapEnts headers; rerun with --map-ents-offset. "
                + json.dumps(details, ensure_ascii=False)
            )
        candidate, entity_text, entities = candidates[0]
        report = {
            "status": "MAPENTS_EXTRACTED",
            "input_file": str(args.map_file),
            "input_bytes": len(data),
            "candidate": asdict(candidate),
            "entities": [
                {
                    "index": index,
                    "classname": fields.get("97"),
                    "targetname": fields.get("642"),
                    "fields": fields,
                }
                for index, fields in enumerate(entities)
            ],
            "field_ids": {
                "97": "classname",
                "47": "angles",
                "430": "origin",
                "392": "model",
                "640": "target",
                "642": "targetname",
            },
        }
        if args.output_ents:
            args.output_ents.parent.mkdir(parents=True, exist_ok=True)
            args.output_ents.write_bytes(entity_text)
            report["output_ents"] = str(args.output_ents)
        rendered = json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True) + "\n"
        if args.output_json:
            args.output_json.parent.mkdir(parents=True, exist_ok=True)
            args.output_json.write_text(rendered, encoding="utf-8")
        print(rendered, end="")
        return 0
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
