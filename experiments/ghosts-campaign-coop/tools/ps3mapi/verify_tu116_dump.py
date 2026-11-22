#!/usr/bin/env python3
"""Verify exact TU 1.16 PS3MAPI segment dumps, then reconstruct an analysis ELF."""
from __future__ import annotations
import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path

try:
    from .reconstruct_elf_from_ps3mapi_dumps import (
        DUMP_CHUNK, PT_LOAD, DumpFormatError, align_up, locate_dump,
        read_self_headers, reconstruct, sha256_bytes,
    )
except ImportError:
    from reconstruct_elf_from_ps3mapi_dumps import (
        DUMP_CHUNK, PT_LOAD, DumpFormatError, align_up, locate_dump,
        read_self_headers, reconstruct, sha256_bytes,
    )

def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()

def verify(
    self_file: Path,
    dump_dir: Path,
    map_file: Path,
    output_elf: Path,
    mode: str,
    *,
    overwrite: bool = False,
) -> dict:
    if mode not in ("campaign", "multiplayer"):
        raise ValueError("mode must be campaign or multiplayer")
    root = json.loads(map_file.read_text(encoding="utf-8"))
    if mode not in root:
        raise ValueError(f"memory map has no {mode!r} profile")
    spec = root[mode]
    self_data, eh, phdrs = read_self_headers(self_file)
    entry = eh[4]
    if f"0x{entry:X}".lower() != str(spec["entry_point"]).lower():
        raise ValueError(f"entry point mismatch: SELF=0x{entry:X}, map={spec['entry_point']}")

    expected = {int(row["segment_index"]): row for row in spec["pt_load_segments"]}
    checked = []
    for index, ph in enumerate(phdrs):
        p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align = ph
        if p_type != PT_LOAD or p_filesz == 0:
            continue
        if index not in expected:
            raise ValueError(f"unexpected non-empty PT_LOAD segment index {index}")
        row = expected[index]
        actual = (p_vaddr, p_filesz, p_memsz, p_align)
        mapped = (
            int(row["virtual_address"], 16), int(row["file_size"]),
            int(row["memory_size"]), int(row["alignment"]),
        )
        if actual != mapped:
            raise ValueError(f"PT_LOAD {index} disagrees with TU 1.16 map: SELF={actual}, map={mapped}")
        dump = locate_dump(dump_dir, index, mode)
        blob = dump.read_bytes()
        expected_size = align_up(p_filesz, DUMP_CHUNK)
        if len(blob) != expected_size:
            raise ValueError(
                f"{dump.name}: {len(blob)} bytes; webMAN PS3MAPI should write exactly {expected_size} bytes"
            )
        unique_values = len(set(blob[:p_filesz]))
        nonzero_fraction = sum(byte != 0 for byte in blob[:p_filesz]) / p_filesz
        if p_flags & 1 and (unique_values < 32 or nonzero_fraction < 0.01):
            raise ValueError(
                f"{dump.name}: executable segment looks empty or invalid "
                f"(unique={unique_values}, nonzero={nonzero_fraction:.3%})"
            )
        entry_bytes = None
        if p_flags & 1 and p_vaddr <= entry < p_vaddr + p_filesz:
            entry_offset = entry - p_vaddr
            entry_bytes = blob[entry_offset:entry_offset + 32].hex()
        checked.append({
            "segment_index": index,
            "virtual_address": f"0x{p_vaddr:X}",
            "file_size": p_filesz,
            "memory_size": p_memsz,
            "dump_file": str(dump),
            "dump_size": len(blob),
            "expected_dump_size": expected_size,
            "dump_sha256": sha256_file(dump),
            "unique_byte_values": unique_values,
            "nonzero_fraction": round(nonzero_fraction, 6),
            "entrypoint_bytes_hex": entry_bytes,
        })
    if set(expected) != {row["segment_index"] for row in checked}:
        raise ValueError("dump set does not contain every expected non-empty PT_LOAD segment")

    rebuilt = reconstruct(
        self_file, dump_dir, output_elf, mode=mode, overwrite=overwrite
    )
    return {
        "status": "PASS_METADATA",
        "mode": mode,
        "title_id": "BLUS31270",
        "target_update": "1.16",
        "version_verification_note": (
            "The SELF program headers match the recorded TU 1.16 map. "
            "This is a structural match, not cryptographic proof of the installed update; "
            "retain the SELF SHA-256 and compare it with the user's verified installed files."
        ),
        "self_file": str(self_file),
        "self_sha256": sha256_bytes(self_data),
        "entry_point": rebuilt["entry_point"],
        "segments": checked,
        "reconstructed_elf": rebuilt,
        "limitations": [
            "A structurally valid dump is not proof that the correct game mode was active; the operator must run the matching manual script.",
            "The process PID is selected by webMAN's current-game context; do not run the Campaign script in Multiplayer or vice versa.",
            "The resulting ELF is for local analysis only and is not a runnable game executable.",
        ],
    }

def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("campaign", "multiplayer"), default="campaign")
    parser.add_argument("--self", dest="self_file", type=Path, required=True)
    parser.add_argument("--dumps", type=Path, required=True)
    parser.add_argument("--map", dest="map_file", type=Path, default=Path(__file__).with_name("SELF_MEMORY_MAP_TU116.json"))
    parser.add_argument("--elf", type=Path, default=Path("exec_campaign_tu116.elf"))
    parser.add_argument("--report", type=Path, default=Path("exec_campaign_tu116_report.json"))
    parser.add_argument("--overwrite", action="store_true")
    args = parser.parse_args(argv)
    try:
        result = verify(
            args.self_file, args.dumps, args.map_file, args.elf, args.mode,
            overwrite=args.overwrite,
        )
    except (OSError, ValueError, DumpFormatError, struct.error) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    rendered = json.dumps(result, indent=2, sort_keys=True)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(rendered + "\n", encoding="utf-8")
    print(rendered)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
