#!/usr/bin/env python3
"""Find client/split-screen-related ASCII strings in a reconstructed PS3 PPU ELF."""
from __future__ import annotations
import argparse
import csv
import json
import re
import struct
from pathlib import Path
from typing import Iterable

ELF_HEADER = struct.Struct(">16sHHIQQQIHHHHHH")
PHDR = struct.Struct(">IIQQQQQQ")
PT_LOAD = 1
DEFAULT_TERMS = (
    "split", "localclient", "local_client", "client", "controller", "gamepad",
    "coop", "co-op", "issplitscreen", "player", "spawn", "connect", "input",
    "camera", "view", "hud", "spectator", "join", "pad", "dvar",
)

class RuntimeElfError(ValueError):
    """Invalid reconstructed PPU ELF."""

def load_segments(path: Path):
    data = path.read_bytes()
    if len(data) < ELF_HEADER.size:
        raise RuntimeElfError("ELF header truncated")
    eh = ELF_HEADER.unpack_from(data)
    ident, etype, machine, version, entry, phoff, shoff, flags, ehsize, phentsize, phnum, shentsize, shnum, shstrndx = eh
    if ident[:4] != b"\x7fELF" or ident[4] != 2 or ident[5] != 2 or machine != 21:
        raise RuntimeElfError("expected ELF64 big-endian PowerPC64")
    if ehsize != ELF_HEADER.size or phentsize < PHDR.size or not 1 <= phnum <= 256:
        raise RuntimeElfError("invalid ELF program-header dimensions")
    if phoff > len(data) or phnum * phentsize > len(data) - phoff:
        raise RuntimeElfError("ELF program-header table outside file")
    segments = []
    for index in range(phnum):
        p = PHDR.unpack_from(data, phoff + index * phentsize)
        p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align = p
        if p_type != PT_LOAD or p_filesz == 0:
            continue
        if p_memsz < p_filesz or p_offset > len(data) or p_filesz > len(data) - p_offset:
            raise RuntimeElfError(f"PT_LOAD {index} has invalid file or memory bounds")
        segments.append((index, p_flags, p_vaddr, data[p_offset:p_offset + p_filesz]))
    if not segments:
        raise RuntimeElfError("ELF has no file-backed PT_LOAD segments")
    return entry, segments

def scan(path: Path, terms: Iterable[str] = DEFAULT_TERMS, min_length: int = 4):
    if min_length < 2 or min_length > 128:
        raise ValueError("min_length must be between 2 and 128")
    entry, segments = load_segments(path)
    rx = re.compile(rb"[\x20-\x7e]{%d,}" % min_length)
    terms_l = tuple(term.lower() for term in terms if term)
    found = {}
    for index, flags, vaddr, blob in segments:
        for match in rx.finditer(blob):
            raw = match.group(0)
            value = raw.decode("ascii", errors="ignore").strip()
            if not value:
                continue
            low = value.lower()
            hits = [term for term in terms_l if term in low]
            if not hits:
                continue
            found.setdefault(value, []).append({
                "segment_index": index,
                "address": f"0x{vaddr + match.start():X}",
                "offset": match.start(),
                "keyword_hits": hits,
                "executable_segment": bool(flags & 1),
            })
    return {
        "file": str(path),
        "entry_point": f"0x{entry:X}",
        "terms": list(terms_l),
        "match_count": sum(len(rows) for rows in found.values()),
        "unique_strings": len(found),
        "matches": [
            {"string": value, "locations": locations}
            for value, locations in sorted(found.items(), key=lambda item: item[0].lower())
        ],
        "interpretation_limit": (
            "String presence is a navigation lead only. It does not prove a function is callable, "
            "active in Campaign, or safe to hook."
        ),
    }

def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    parser.add_argument("--terms", default=",".join(DEFAULT_TERMS))
    parser.add_argument("--min-length", type=int, default=4)
    parser.add_argument("--json", type=Path)
    parser.add_argument("--csv", type=Path)
    args = parser.parse_args(argv)
    try:
        result = scan(
            args.elf,
            tuple(value.strip() for value in args.terms.split(",") if value.strip()),
            args.min_length,
        )
    except (OSError, RuntimeElfError, ValueError) as exc:
        parser.error(str(exc))
    rendered = json.dumps(result, indent=2, sort_keys=True)
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(rendered + "\n", encoding="utf-8")
    if args.csv:
        args.csv.parent.mkdir(parents=True, exist_ok=True)
        with args.csv.open("w", encoding="utf-8-sig", newline="") as stream:
            writer = csv.writer(stream)
            writer.writerow(["string", "segment_index", "address", "offset", "keyword_hits", "executable_segment"])
            for item in result["matches"]:
                for location in item["locations"]:
                    writer.writerow([
                        item["string"], location["segment_index"], location["address"],
                        location["offset"], ";".join(location["keyword_hits"]),
                        location["executable_segment"],
                    ])
    print(rendered)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
