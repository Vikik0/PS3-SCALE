#!/usr/bin/env python3
"""Extend the PRX PT_LOAD bounds to include every SHF_ALLOC section.

The pinned PS3DK linker script can emit a large NOBITS .bss section whose
section header extends past PT_LOAD.p_memsz. prx-gen correctly rejects
relocations into that tail. The PRX loader allocates from p_memsz, so update
the segment bounds from the actual allocated section headers before prx-gen.
"""
from pathlib import Path
import struct
import sys

ELF_HEADER_SIZE = 64
PROGRAM_HEADER_SIZE = 56
SECTION_HEADER_SIZE = 64
PT_LOAD = 1
SHT_NOBITS = 8
SHF_ALLOC = 0x2


def u16(data: bytearray, offset: int) -> int:
    return struct.unpack_from(">H", data, offset)[0]


def u32(data: bytearray, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def u64(data: bytearray, offset: int) -> int:
    return struct.unpack_from(">Q", data, offset)[0]


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: patch_prx_load_segment.py <unstripped-elf>", file=sys.stderr)
        return 2

    path = Path(sys.argv[1])
    data = bytearray(path.read_bytes())
    if len(data) < ELF_HEADER_SIZE or data[:4] != b"\x7fELF":
        raise SystemExit("not an ELF file")
    if data[4] != 2 or data[5] != 2:
        raise SystemExit("expected ELF64 big-endian PPU output")

    phoff = u64(data, 32)
    shoff = u64(data, 40)
    phentsize = u16(data, 54)
    phnum = u16(data, 56)
    shentsize = u16(data, 58)
    shnum = u16(data, 60)
    if phentsize != PROGRAM_HEADER_SIZE or shentsize != SECTION_HEADER_SIZE:
        raise SystemExit("unexpected ELF header entry sizes")
    if phoff + phentsize * phnum > len(data) or shoff + shentsize * shnum > len(data):
        raise SystemExit("ELF header tables are outside the file")

    loads = []
    for index in range(phnum):
        offset = phoff + index * phentsize
        if u32(data, offset) == PT_LOAD:
            loads.append(offset)
    if len(loads) != 1:
        raise SystemExit(f"expected exactly one PT_LOAD, found {len(loads)}")

    load = loads[0]
    p_offset = u64(data, load + 8)
    p_vaddr = u64(data, load + 16)
    old_filesz = u64(data, load + 32)
    old_memsz = u64(data, load + 40)
    required_filesz = old_filesz
    required_memsz = old_memsz
    alloc_sections = 0

    for index in range(shnum):
        offset = shoff + index * shentsize
        sh_type = u32(data, offset + 4)
        sh_flags = u64(data, offset + 8)
        sh_addr = u64(data, offset + 16)
        sh_offset = u64(data, offset + 24)
        sh_size = u64(data, offset + 32)
        if not (sh_flags & SHF_ALLOC) or sh_size == 0:
            continue
        alloc_sections += 1
        if sh_addr < p_vaddr:
            raise SystemExit(f"allocated section #{index} starts before PT_LOAD")
        required_memsz = max(required_memsz, sh_addr + sh_size - p_vaddr)
        if sh_type != SHT_NOBITS:
            if sh_offset < p_offset:
                raise SystemExit(f"file-backed allocated section #{index} starts before PT_LOAD")
            required_filesz = max(required_filesz, sh_offset + sh_size - p_offset)

    if required_memsz < required_filesz:
        required_memsz = required_filesz

    struct.pack_into(">Q", data, load + 32, required_filesz)
    struct.pack_into(">Q", data, load + 40, required_memsz)
    path.write_bytes(data)

    print(
        "PRX PT_LOAD bounds: "
        f"filesz 0x{old_filesz:x}->0x{required_filesz:x}, "
        f"memsz 0x{old_memsz:x}->0x{required_memsz:x}, "
        f"{alloc_sections} allocated sections"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
