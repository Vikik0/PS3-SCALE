#!/usr/bin/env python3
"""Rebuild a minimal ELF64 BE PPU analysis image from read-only PS3MAPI dumps.

The SELF supplies the original ELF/program headers. PS3MAPI supplies the
loader-mapped segment bytes. webMAN writes process dumps in 64 KiB chunks, so
the default path requires each dump to have exactly align_up(p_filesz, 0x10000)
bytes. BSS is represented by p_memsz and is not fabricated.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path

SCE_MAGIC = b"SCE\0"
ELF_MAGIC = b"\x7fELF"
SCE_HEADER = struct.Struct(">4sIHHIQQ")
EXT_HEADER = struct.Struct(">10Q")
ELF_HEADER = struct.Struct(">16sHHIQQQIHHHHHH")
ELF64_PHDR = struct.Struct(">IIQQQQQQ")
PT_LOAD = 1
PF_X = 1
DUMP_CHUNK = 0x10000
EM_PPC64 = 21
MODE_PREFIX = {
    "campaign": "GHOSTS_TU116_CAMPAIGN_SEG",
    "multiplayer": "GHOSTS_TU116_MULTIPLAYER_SEG",
}

class DumpFormatError(ValueError):
    """Invalid SELF metadata or incomplete process dump."""

def align_up(value: int, alignment: int) -> int:
    if alignment <= 1:
        return value
    return (value + alignment - 1) // alignment * alignment

def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()

def _read_range(data: bytes, offset: int, size: int, label: str) -> bytes:
    if offset < 0 or size < 0 or offset > len(data) or size > len(data) - offset:
        raise DumpFormatError(f"{label} points outside the SELF file")
    return data[offset:offset + size]

def read_self_headers(path: Path):
    data = path.read_bytes()
    if len(data) < 0x70:
        raise DumpFormatError("SELF header is truncated")
    magic, version, keyrev, header_type, metadata_offset, header_len, elf_size = SCE_HEADER.unpack_from(data)
    if magic != SCE_MAGIC or version != 2:
        raise DumpFormatError("input is not a supported PS3 SELF v2")
    if header_type != 1:
        raise DumpFormatError(f"SELF header type is {header_type}; expected 1")
    if header_len < 0x70 or header_len > len(data):
        raise DumpFormatError("SELF header length is outside the file")
    if metadata_offset < 0x20 or metadata_offset >= len(data):
        raise DumpFormatError("SELF metadata offset is outside the file")

    ext = EXT_HEADER.unpack(_read_range(data, 0x20, EXT_HEADER.size, "extended SELF header"))
    (_, app_info_off, elf_off, phdr_off, _, _, _, _, _, _) = ext
    eh = ELF_HEADER.unpack(_read_range(data, elf_off, ELF_HEADER.size, "embedded ELF header"))
    ident, etype, machine, ever, entry, phoff, shoff, flags, ehsize, phentsize, phnum, shentsize, shnum, shstrndx = eh
    if ident[:4] != ELF_MAGIC or ident[4] != 2 or ident[5] != 2 or machine != EM_PPC64:
        raise DumpFormatError("embedded ELF is not ELF64 big-endian PowerPC64")
    if ehsize != ELF_HEADER.size or phentsize < ELF64_PHDR.size or not 1 <= phnum <= 256:
        raise DumpFormatError("invalid ELF header/program-header dimensions")
    _read_range(data, phdr_off, phentsize * phnum, "SELF program-header table")

    phdrs = []
    for index in range(phnum):
        raw = _read_range(data, phdr_off + index * phentsize, ELF64_PHDR.size, f"program header {index}")
        phdrs.append(ELF64_PHDR.unpack(raw))
    return data, eh, phdrs

def locate_dump(dump_dir: Path, segment_index: int, mode: str | None = None, prefix: str | None = None) -> Path:
    names = []
    if prefix:
        names.append(f"{prefix}{segment_index}.bin")
    elif mode in MODE_PREFIX:
        names.append(f"{MODE_PREFIX[mode]}{segment_index}.bin")
    names.append(f"SEG{segment_index}.bin")
    for name in names:
        candidate = dump_dir / name
        if candidate.is_file():
            return candidate
    raise FileNotFoundError(
        f"missing PT_LOAD {segment_index} dump; expected one of: {', '.join(names)}"
    )

def reconstruct(
    self_path: Path,
    dump_dir: Path,
    output: Path,
    *,
    mode: str | None = None,
    prefix: str | None = None,
    allow_unpadded: bool = False,
    overwrite: bool = False,
) -> dict:
    self_data, eh, phdrs = read_self_headers(self_path)
    ident, etype, machine, ever, entry, phoff, shoff, flags, ehsize, phentsize, phnum, shentsize, shnum, shstrndx = eh

    if output.resolve() == self_path.resolve():
        raise DumpFormatError("output ELF path must not overwrite the input SELF")
    loads = []
    for index, ph in enumerate(phdrs):
        p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align = ph
        if p_type != PT_LOAD or p_filesz == 0:
            continue
        if p_memsz < p_filesz:
            raise DumpFormatError(f"PT_LOAD {index}: p_memsz is smaller than p_filesz")
        if p_align > 1 and (p_align & (p_align - 1)):
            raise DumpFormatError(f"PT_LOAD {index}: p_align is not a power of two")
        dump_path = locate_dump(dump_dir, index, mode, prefix)
        if dump_path.resolve() == output.resolve() or dump_path.resolve() == self_path.resolve():
            raise DumpFormatError("output path must not overwrite an input file")
        blob = dump_path.read_bytes()
        expected_size = align_up(p_filesz, DUMP_CHUNK)
        if len(blob) < p_filesz:
            raise DumpFormatError(f"{dump_path.name} has {len(blob)} bytes; needs {p_filesz}")
        if not allow_unpadded and len(blob) != expected_size:
            raise DumpFormatError(
                f"{dump_path.name} has {len(blob)} bytes; expected the webMAN 64 KiB-padded size {expected_size}"
            )
        if allow_unpadded and len(blob) not in (p_filesz, expected_size):
            raise DumpFormatError(
                f"{dump_path.name} has {len(blob)} bytes; expected {p_filesz} or {expected_size}"
            )
        loads.append({
            "index": index, "flags": p_flags, "vaddr": p_vaddr, "paddr": p_paddr,
            "filesz": p_filesz, "memsz": p_memsz, "align": max(1, p_align),
            "data": blob[:p_filesz], "dump_file": str(dump_path),
            "dump_size": len(blob), "expected_dump_size": expected_size,
            "dump_sha256": sha256_bytes(blob),
        })

    if not loads:
        raise DumpFormatError("SELF has no non-empty PT_LOAD segments")
    if not any(
        (segment["flags"] & PF_X)
        and segment["vaddr"] <= entry < segment["vaddr"] + segment["filesz"]
        for segment in loads
    ):
        raise DumpFormatError("ELF entry point is not inside file-backed executable PT_LOAD bytes")

    ordered = sorted(loads, key=lambda item: item["vaddr"])
    for previous, current in zip(ordered, ordered[1:]):
        if previous["vaddr"] + previous["memsz"] > current["vaddr"]:
            raise DumpFormatError(
                f"PT_LOAD virtual ranges overlap: segment {previous['index']} and {current['index']}"
            )

    if output.exists() and not overwrite:
        raise FileExistsError(f"output already exists: {output}; pass --overwrite to replace it")

    out_phoff = ELF_HEADER.size
    out_phentsize = ELF64_PHDR.size
    cursor = DUMP_CHUNK
    for segment in loads:
        cursor = align_up(cursor, segment["align"])
        segment["out_offset"] = cursor
        cursor += segment["filesz"]

    out = bytearray(cursor)
    new_header = ELF_HEADER.pack(
        bytes(ident), etype, machine, ever, entry, out_phoff, 0, flags,
        ELF_HEADER.size, out_phentsize, len(loads), 0, 0, 0,
    )
    out[:len(new_header)] = new_header
    segment_report = []
    for out_index, segment in enumerate(loads):
        ph = ELF64_PHDR.pack(
            PT_LOAD, segment["flags"], segment["out_offset"], segment["vaddr"],
            segment["paddr"], segment["filesz"], segment["memsz"], segment["align"],
        )
        ph_start = out_phoff + out_index * out_phentsize
        out[ph_start:ph_start + len(ph)] = ph
        start = segment["out_offset"]
        out[start:start + segment["filesz"]] = segment["data"]
        segment_report.append({
            key: segment[key] for key in (
                "index", "flags", "vaddr", "paddr", "filesz", "memsz", "align",
                "out_offset", "dump_file", "dump_size", "expected_dump_size", "dump_sha256",
            )
        })

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(out)
    return {
        "status": "RECONSTRUCTED_ANALYSIS_ELF",
        "mode": mode,
        "input_self": str(self_path),
        "input_self_sha256": sha256_bytes(self_data),
        "output_elf": str(output),
        "output_size": len(out),
        "sha256": sha256_bytes(out),
        "entry_point": f"0x{entry:X}",
        "machine": "PowerPC64",
        "endianness": "big",
        "program_header_count": len(loads),
        "segments": segment_report,
        "limitations": [
            "Runtime bytes are copied from the user's active game process; they may include loader relocations.",
            "Section headers and non-PT_LOAD metadata are omitted; PT_LOAD headers and entry point are retained.",
            "Zero-fill BSS is represented by p_memsz and is not copied as fabricated file bytes.",
            "The reconstructed ELF is for local static analysis, not a runnable replacement EBOOT.",
        ],
    }

def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("self_file", type=Path)
    parser.add_argument("dump_dir", type=Path)
    parser.add_argument("output_elf", type=Path)
    parser.add_argument("--mode", choices=("campaign", "multiplayer"), required=True)
    parser.add_argument("--prefix", help="optional dump filename prefix; default is selected from --mode")
    parser.add_argument("--allow-unpadded", action="store_true", help="accept exact p_filesz dumps as well as 64 KiB-padded PS3MAPI dumps")
    parser.add_argument("--overwrite", action="store_true")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args(argv)
    try:
        result = reconstruct(
            args.self_file, args.dump_dir, args.output_elf, mode=args.mode,
            prefix=args.prefix, allow_unpadded=args.allow_unpadded, overwrite=args.overwrite,
        )
    except (OSError, DumpFormatError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    rendered = json.dumps(result, indent=2, sort_keys=True)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(rendered + "\n", encoding="utf-8")
    print(rendered)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
