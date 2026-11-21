#!/usr/bin/env python3
"""Read-only inspector for retail PS3 SELF/PRX containers.

Reads public container metadata only. It does not decrypt SELF files, supply
keys, modify executables, or infer a game Title ID / Title Update.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path
from typing import Any

SCE_MAGIC = b"SCE\\x00"
ELF_MAGIC = b"\\x7fELF"
SCE_HEADER = struct.Struct(">4sIHHIQQ")
EXT_HEADER = struct.Struct(">10Q")
APP_INFO = struct.Struct(">QIIQQ")
ELF_HEADER = struct.Struct(">16sHHIQQQIHHHHHH")
ELF64_PHDR = struct.Struct(">IIQQQQQQ")
SEGMENT_INFO = struct.Struct(">QQQQ")

class SelfFormatError(ValueError):
    """Invalid or unsupported SELF container."""

def _range(data: bytes, offset: int, size: int, what: str) -> bytes:
    if offset < 0 or size < 0 or offset > len(data) or size > len(data) - offset:
        raise SelfFormatError(f"{what} is outside the file")
    return data[offset:offset + size]

def inspect_self(data: bytes, name: str = "<memory>") -> dict[str, Any]:
    if len(data) < SCE_HEADER.size:
        raise SelfFormatError("file is shorter than the 0x20-byte SCE header")
    magic, version, key_revision, header_type, metadata_offset, header_len, elf_size = SCE_HEADER.unpack_from(data)
    if magic != SCE_MAGIC:
        raise SelfFormatError("missing SCE\\0 magic; input is not a PS3 SELF container")
    if version != 2:
        raise SelfFormatError(f"unsupported PS3 SELF header version: {version}")
    if header_type != 1:
        raise SelfFormatError(f"expected SELF header type 1, found {header_type}")
    if header_len < 0x20 or header_len > len(data):
        raise SelfFormatError("SELF header length is outside the file")
    if metadata_offset < 0x20 or metadata_offset >= len(data):
        raise SelfFormatError("metadata offset is outside the file")

    ext = EXT_HEADER.unpack(_range(data, 0x20, EXT_HEADER.size, "extended SELF header"))
    (unknown, app_info_offset, elf_offset, phdr_offset, shdr_offset,
     segment_info_offset, version_info_offset, control_info_offset,
     control_info_size, padding) = ext
    auth_id, vendor_id, program_type, app_version, app_padding = APP_INFO.unpack(
        _range(data, app_info_offset, APP_INFO.size, "SELF app-info")
    )
    elf = ELF_HEADER.unpack(_range(data, elf_offset, ELF_HEADER.size, "embedded ELF header"))
    (ident, elf_type, machine, elf_version, entry, phoff, shoff, flags,
     ehsize, phentsize, phnum, shentsize, shnum, shstrndx) = elf
    if ident[:4] != ELF_MAGIC:
        raise SelfFormatError("embedded payload does not start with ELF magic")
    if ident[4] != 2 or ident[5] != 2:
        raise SelfFormatError("embedded payload is not ELF64 big-endian")
    if machine != 21:
        raise SelfFormatError(f"embedded ELF machine is {machine}, not PowerPC64 (21)")
    if phentsize < ELF64_PHDR.size:
        raise SelfFormatError("ELF program-header entry size is too small")
    if phnum > 256:
        raise SelfFormatError("implausible ELF program-header count")

    _range(data, phdr_offset, phentsize * phnum, "SELF program-header table")
    _range(data, segment_info_offset, SEGMENT_INFO.size * phnum, "SELF segment-info table")
    segments = []
    encrypted_count = 0
    nonempty_count = 0
    for i in range(phnum):
        ph = ELF64_PHDR.unpack(_range(data, phdr_offset + i * phentsize,
                                     ELF64_PHDR.size, f"program header {i}"))
        p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align = ph
        data_offset, data_size, compression_raw, encryption_raw = SEGMENT_INFO.unpack(
            _range(data, segment_info_offset + i * SEGMENT_INFO.size,
                   SEGMENT_INFO.size, f"segment info {i}")
        )
        if data_size:
            _range(data, data_offset, data_size, f"container segment data {i}")
            nonempty_count += 1
        encrypted = bool(data_size and encryption_raw)
        encrypted_count += int(encrypted)
        segments.append({
            "index": i, "program_type": p_type, "flags": p_flags,
            "elf_file_offset": p_offset, "virtual_address": f"0x{p_vaddr:X}",
            "elf_file_size": p_filesz, "memory_size": p_memsz, "alignment": p_align,
            "container_data_offset": data_offset, "container_data_size": data_size,
            "compression_field_raw": f"0x{compression_raw:X}",
            "encryption_field_raw": f"0x{encryption_raw:X}", "encrypted": encrypted,
        })

    return {
        "file": name, "file_size": len(data), "sha256": hashlib.sha256(data).hexdigest(),
        "container": {
            "magic": "SCE\\0", "header_version": version, "key_revision": key_revision,
            "header_type": header_type, "metadata_offset": metadata_offset,
            "header_length": header_len, "embedded_elf_size": elf_size,
            "extended_header": {
                "unknown": unknown, "app_info_offset": app_info_offset,
                "elf_offset": elf_offset, "program_header_offset": phdr_offset,
                "section_header_offset": shdr_offset, "segment_info_offset": segment_info_offset,
                "version_info_offset": version_info_offset, "control_info_offset": control_info_offset,
                "control_info_size": control_info_size, "padding": padding,
            },
            "app_info": {
                "auth_id": f"0x{auth_id:016X}", "vendor_id": f"0x{vendor_id:08X}",
                "program_type": program_type, "app_version": f"0x{app_version:016X}",
                "padding": app_padding,
            },
        },
        "embedded_elf_header": {
            "class": "ELF64", "endianness": "big", "osabi": ident[7],
            "type": elf_type, "machine": "PowerPC64", "elf_version": elf_version,
            "entry_point": f"0x{entry:X}", "program_header_count": phnum,
            "section_header_count": shnum, "section_name_table_index": shstrndx,
        },
        "segment_summary": {
            "program_header_count": phnum, "nonempty_container_segments": nonempty_count,
            "encrypted_segments": encrypted_count, "all_container_ranges_valid": True,
        },
        "segments": segments,
        "limitations": [
            "Reads SELF container metadata only; does not decrypt encrypted segments.",
            "SELF headers do not establish the game Title ID or Title Update number.",
            "A valid SELF header does not prove compatibility with a particular CFW or game build.",
        ],
    }

def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("files", nargs="+", help="SELF/EBOOT files to inspect")
    parser.add_argument("--output", help="write JSON report to this path")
    args = parser.parse_args(argv)
    try:
        reports = [inspect_self(Path(filename).read_bytes(), filename) for filename in args.files]
    except (OSError, SelfFormatError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    output = json.dumps(reports, indent=2)
    if args.output:
        Path(args.output).write_text(output + "\\n", encoding="utf-8")
    else:
        print(output)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
