#!/usr/bin/env python3
"""Read-only inspector for a user-supplied decrypted PS3 PPU ELF.

This tool does not decrypt SELF files, modify binaries, or install anything.
It fingerprints an ELF and maps explicitly reported TU 1.16 candidate offsets
against executable PT_LOAD segments. An optional image base is never guessed.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any

ELF_MAGIC = b"\x7fELF"
ELFCLASS64 = 2
ELFDATA2MSB = 2
EV_CURRENT = 1
EM_PPC64 = 21
ET_EXEC = 2
ET_DYN = 3
PT_LOAD = 1
PF_X = 1
PF_W = 2
PF_R = 4
SHT_SYMTAB = 2
SHT_DYNSYM = 11
STT_FUNC = 2

# Addresses are leads copied from the repository's candidate-spawn document.
# They are not verified against any particular retail EBOOT or Campaign.
CANDIDATES = (
    ("G_Spawn", 0x290E58, "entity allocation"),
    ("G_CallSpawnEntity", 0x28AB00, "entity spawn dispatcher"),
    ("G_SetModel", 0x28FA4C, "model assignment"),
    ("SP_Script_Model", 0x286710, "script model spawn helper"),
    ("SV_LinkEntity", 0x32C420, "world entity linking"),
    ("SV_UnlinkEntity", 0x32C3A0, "world entity unlinking"),
    ("PlayerCMD_Spawn", 0x23D71C, "player script command; VM context unknown"),
    ("PlayerCMD_ClonePlayer", 0x23D850, "clone/corpse path; not a second local player"),
    ("PlayerCMD_IsSplitScreenPlayer", 0x23EA54, "split-screen identity query"),
    ("PlayerCMD_IsSplitScreenPlayerPrimary", 0x23EB38, "primary split-screen identity query"),
    ("PlayerCMD_SetOrigin", 0x23585C, "script actor position command"),
    ("PlayerCMD_SetVelocity", 0x235958, "script actor velocity command"),
    ("PlayerCMD_GiveWeapon", 0x233468, "script weapon assignment command"),
)

TITLE_IDS = {
    "BLUS31270", "NPUB31301", "NPUB50021", "BLES01945", "BLES01948",
    "NPEB01832", "NPEB01835", "BLJM61125", "BLJM61126", "NPJB00528",
}


class ElfFormatError(ValueError):
    """Raised when the supplied file is not a supported ELF image."""


@dataclass(frozen=True)
class Segment:
    index: int
    kind: int
    flags: int
    file_offset: int
    virtual_address: int
    file_size: int
    memory_size: int
    alignment: int

    @property
    def executable(self) -> bool:
        return bool(self.flags & PF_X)

    @property
    def permissions(self) -> str:
        return "".join(("r" if self.flags & PF_R else "-",
                        "w" if self.flags & PF_W else "-",
                        "x" if self.flags & PF_X else "-"))


@dataclass(frozen=True)
class Mapping:
    virtual_address: int
    file_offset: int
    segment_index: int
    permissions: str
    executable: bool


class PpuElf:
    def __init__(self, data: bytes):
        self.data = data
        self.header = self._parse_header()
        self.segments = self._parse_segments()
        self.sections = self._parse_sections()

    def _parse_header(self) -> dict[str, int]:
        if len(self.data) < 64:
            raise ElfFormatError("file is shorter than an ELF64 header")
        ident = self.data[:16]
        if ident[:4] != ELF_MAGIC:
            raise ElfFormatError("missing ELF magic; input may still be a SELF container")
        if ident[4] != ELFCLASS64:
            raise ElfFormatError("expected ELF64 (class 2)")
        if ident[5] != ELFDATA2MSB:
            raise ElfFormatError("expected big-endian PPU ELF (data encoding 2)")
        if ident[6] != EV_CURRENT:
            raise ElfFormatError("unsupported ELF identification version")

        values = struct.unpack_from(">16sHHIQQQIHHHHHH", self.data, 0)
        (ident, e_type, e_machine, e_version, e_entry, e_phoff, e_shoff,
         e_flags, e_ehsize, e_phentsize, e_phnum, e_shentsize,
         e_shnum, e_shstrndx) = values
        if e_machine != EM_PPC64:
            raise ElfFormatError(f"expected EM_PPC64 ({EM_PPC64}), found {e_machine}")
        if e_type not in (ET_EXEC, ET_DYN):
            raise ElfFormatError(f"expected ET_EXEC or ET_DYN, found ELF type {e_type}")
        if e_version != EV_CURRENT or e_ehsize < 64:
            raise ElfFormatError("invalid ELF header version or size")
        if e_phnum and e_phentsize < 56:
            raise ElfFormatError("program-header entries are smaller than ELF64 minimum")
        if e_shnum and e_shentsize < 64:
            raise ElfFormatError("section-header entries are smaller than ELF64 minimum")
        self._check_range(e_phoff, e_phentsize * e_phnum, "program-header table")
        self._check_range(e_shoff, e_shentsize * e_shnum, "section-header table")
        return {
            "type": e_type, "machine": e_machine, "entry": e_entry,
            "program_header_offset": e_phoff, "program_header_entry_size": e_phentsize,
            "program_header_count": e_phnum, "section_header_offset": e_shoff,
            "section_header_entry_size": e_shentsize, "section_header_count": e_shnum,
            "section_name_table_index": e_shstrndx, "flags": e_flags,
        }

    def _check_range(self, offset: int, size: int, label: str) -> None:
        if offset < 0 or size < 0 or offset > len(self.data) or size > len(self.data) - offset:
            raise ElfFormatError(f"{label} points outside the file")

    def _parse_segments(self) -> list[Segment]:
        result: list[Segment] = []
        offset = self.header["program_header_offset"]
        entry_size = self.header["program_header_entry_size"]
        for index in range(self.header["program_header_count"]):
            pos = offset + index * entry_size
            fields = struct.unpack_from(">IIQQQQQQ", self.data, pos)
            kind, flags, file_offset, vaddr, _paddr, filesz, memsz, align = fields
            self._check_range(file_offset, filesz, f"segment {index}")
            if filesz > memsz:
                raise ElfFormatError(f"segment {index} has file_size greater than memory_size")
            result.append(Segment(index, kind, flags, file_offset, vaddr, filesz, memsz, align))
        return result

    def _parse_sections(self) -> list[dict[str, Any]]:
        count = self.header["section_header_count"]
        if not count:
            return []
        offset = self.header["section_header_offset"]
        entry_size = self.header["section_header_entry_size"]
        raw: list[tuple[int, ...]] = []
        for index in range(count):
            pos = offset + index * entry_size
            raw.append(struct.unpack_from(">IIQQQQIIQQ", self.data, pos))

        names_index = self.header["section_name_table_index"]
        names = b""
        if names_index < len(raw):
            name_section = raw[names_index]
            name_offset, name_size = name_section[4], name_section[5]
            self._check_range(name_offset, name_size, "section-name string table")
            names = self.data[name_offset:name_offset + name_size]

        result = []
        for index, fields in enumerate(raw):
            (name_off, section_type, flags, addr, file_off, size, link,
             info, alignment, entry_size_value) = fields
            if section_type != 8:  # SHT_NOBITS has no file payload
                self._check_range(file_off, size, f"section {index}")
            name = self._cstring(names, name_off) if names and name_off < len(names) else ""
            result.append({
                "index": index, "name": name, "type": section_type,
                "flags": flags, "address": addr, "file_offset": file_off,
                "size": size, "link": link, "entry_size": entry_size_value,
            })
        return result

    @staticmethod
    def _cstring(table: bytes, offset: int) -> str:
        end = table.find(b"\0", offset)
        if end < 0:
            end = len(table)
        return table[offset:end].decode("utf-8", errors="replace")

    def map_virtual_address(self, address: int) -> Mapping | None:
        matches: list[Mapping] = []
        for segment in self.segments:
            if segment.kind != PT_LOAD or not (segment.virtual_address <= address < segment.virtual_address + segment.file_size):
                continue
            matches.append(Mapping(
                virtual_address=address,
                file_offset=segment.file_offset + address - segment.virtual_address,
                segment_index=segment.index,
                permissions=segment.permissions,
                executable=segment.executable,
            ))
        if len(matches) > 1:
            raise ElfFormatError(f"virtual address 0x{address:X} maps to overlapping PT_LOAD segments")
        return matches[0] if matches else None

    def executable_ranges(self) -> list[tuple[int, int]]:
        return [
            (segment.virtual_address, segment.virtual_address + segment.file_size)
            for segment in self.segments
            if segment.kind == PT_LOAD and segment.executable and segment.file_size
        ]

    def read_virtual(self, address: int, size: int) -> bytes | None:
        mapping = self.map_virtual_address(address)
        if mapping is None:
            return None
        segment = self.segments[mapping.segment_index]
        delta = address - segment.virtual_address
        if delta + size > segment.file_size:
            return None
        return self.data[mapping.file_offset:mapping.file_offset + size]

    def opd_info(self) -> dict[str, Any]:
        """Read the PS3 PPU 8-byte {u32 code, u32 TOC} function descriptors."""
        entry = self.header["entry"]
        containing = [
            section for section in self.sections
            if section["size"] and section["address"] <= entry < section["address"] + section["size"]
        ]
        opd_section = min(containing, key=lambda section: section["size"]) if containing else None
        entry_bytes = self.read_virtual(entry, 8)
        entry_descriptor = None
        ranges = self.executable_ranges()

        def is_code(address: int) -> bool:
            return any(lo <= address < hi for lo, hi in ranges)

        if entry_bytes:
            code, toc = struct.unpack(">II", entry_bytes)
            if is_code(code):
                entry_descriptor = {"opd_address": f"0x{entry:X}", "code_address": f"0x{code:X}", "toc": f"0x{toc:X}"}

        descriptors: list[dict[str, Any]] = []
        toc_counts: dict[int, int] = {}
        if opd_section is not None:
            blob = self.read_virtual(opd_section["address"], opd_section["size"])
            if blob is not None:
                for offset in range(0, len(blob) - 7, 8):
                    code, toc = struct.unpack_from(">II", blob, offset)
                    if code == 0 and toc == 0:
                        continue
                    if not is_code(code):
                        continue
                    descriptors.append({
                        "opd_address": f"0x{opd_section['address'] + offset:X}",
                        "code_address": f"0x{code:X}",
                        "toc": f"0x{toc:X}",
                    })
                    toc_counts[toc] = toc_counts.get(toc, 0) + 1

        module_toc = max(toc_counts, key=toc_counts.get) if toc_counts else None
        return {
            "entry_descriptor": entry_descriptor,
            "section_index": opd_section["index"] if opd_section else None,
            "section_name": opd_section["name"] if opd_section else None,
            "section_address": f"0x{opd_section['address']:X}" if opd_section else None,
            "section_size": f"0x{opd_section['size']:X}" if opd_section else None,
            "descriptor_count": len(descriptors),
            "module_toc": f"0x{module_toc:X}" if module_toc is not None else None,
            "descriptors": descriptors,
            "note": "OPD entries are address-taken function descriptors, not guaranteed to enumerate every function in the executable.",
        }

    def function_symbols(self) -> list[dict[str, Any]]:
        result: list[dict[str, Any]] = []
        for section in self.sections:
            if section["type"] not in (SHT_SYMTAB, SHT_DYNSYM):
                continue
            link = section["link"]
            if link >= len(self.sections):
                continue
            strings_section = self.sections[link]
            strings = self.data[strings_section["file_offset"]:strings_section["file_offset"] + strings_section["size"]]
            entsize = section["entry_size"] or 24
            if entsize < 24:
                continue
            start = section["file_offset"]
            end = start + section["size"]
            for pos in range(start, end - 23, entsize):
                name_off, info, _other, shndx, value, size = struct.unpack_from(">IBBHQQ", self.data, pos)
                if (info & 0x0F) != STT_FUNC or shndx == 0 or name_off >= len(strings):
                    continue
                name = self._cstring(strings, name_off)
                if name:
                    result.append({"name": name, "address": value, "size": size})
        return result


def inspect_file(
    file_path: Path,
    title_id: str,
    title_update: str = "1.16",
    firmware_family: str = "CEX",
    firmware_version: str = "4.93",
    image_base: int | None = None,
    max_bytes: int = 32,
) -> dict[str, Any]:
    if title_id not in TITLE_IDS:
        raise ValueError(f"unsupported or unregistered Ghosts PS3 Title ID: {title_id}")
    if title_update != "1.16":
        raise ValueError("this inspector currently records only the TU 1.16 research profile")
    if firmware_family != "CEX" or firmware_version != "4.93":
        raise ValueError("this branch currently targets CEX 4.93 only")
    if max_bytes < 4 or max_bytes > 256:
        raise ValueError("max_bytes must be between 4 and 256")

    data = file_path.read_bytes()
    elf = PpuElf(data)
    digest = hashlib.sha256(data).hexdigest()
    candidates: list[dict[str, Any]] = []
    opd = elf.opd_info()
    opd_code_addresses = sorted(int(item["code_address"], 16) for item in opd["descriptors"])
    function_symbols = elf.function_symbols()

    def describe_function(address: int) -> dict[str, Any]:
        exact = [symbol for symbol in function_symbols if symbol["address"] == address]
        prior = [candidate for candidate in opd_code_addresses if candidate <= address]
        return {
            "exact_symbols": exact,
            "nearest_opd_function_start": f"0x{prior[-1]:X}" if prior else None,
            "distance_from_nearest_opd_start": address - prior[-1] if prior else None,
            "warning": "nearest OPD entry is only a navigation hint; OPD does not list every function",
        }

    for name, reported_offset, purpose in CANDIDATES:
        entry: dict[str, Any] = {
            "name": name,
            "reported_offset": f"0x{reported_offset:X}",
            "purpose": purpose,
            "direct_virtual_address": None,
            "rva_with_image_base": None,
            "mapping_status": "unmapped",
        }
        addresses = [("direct_virtual_address", reported_offset)]
        if image_base is not None:
            addresses.append(("rva_with_image_base", image_base + reported_offset))

        for label, address in addresses:
            mapping = elf.map_virtual_address(address)
            if mapping is None:
                continue
            raw = data[mapping.file_offset:mapping.file_offset + max_bytes]
            entry[label] = {
                **asdict(mapping),
                "file_offset": f"0x{mapping.file_offset:X}",
                "virtual_address": f"0x{mapping.virtual_address:X}",
                "bytes": raw.hex(),
                "function_context": describe_function(address) if mapping.executable else None,
            }
            if mapping.executable:
                entry["mapping_status"] = "mapped_executable"
            else:
                entry["mapping_status"] = "mapped_non_executable"
        candidates.append(entry)

    return {
        "tool": "ps3_elf_inspect",
        "tool_version": "0.1.0",
        "input": {
            "file_name": file_path.name,
            "size_bytes": len(data),
            "sha256": digest,
            "privacy": "binary contents are not copied into this report",
        },
        "operator_asserted_profile": {
            "title_id": title_id,
            "title_update": title_update,
            "firmware_family": firmware_family,
            "firmware_version": firmware_version,
            "warning": "These values are supplied by the operator; ELF metadata does not prove the installed game identity.",
        },
        "elf": {
            **elf.header,
            "type_name": "ET_EXEC" if elf.header["type"] == ET_EXEC else "ET_DYN",
            "machine_name": "EM_PPC64",
            "endianness": "big",
            "entry_hex": f"0x{elf.header['entry']:X}",
            "segments": [
                {**asdict(s), "permissions": s.permissions, "executable": s.executable}
                for s in elf.segments
            ],
            "sections": elf.sections,
            "named_function_symbols": function_symbols,
            "opd": opd,
        },
        "candidate_address_interpretation": {
            "image_base": f"0x{image_base:X}" if image_base is not None else None,
            "note": "Direct VA and RVA+image-base are tested separately. No image base is inferred.",
        },
        "candidate_offsets": candidates,
        "safety": [
            "Read-only inspection only; no bytes are changed.",
            "This tool does not decrypt SELF containers or bypass console security.",
            "A mapped address is not proof of a correct function, calling convention, or Campaign safety.",
            "Do not call or patch a candidate until it is independently verified in the exact EBOOT and runtime.",
        ],
    }


def _parse_int(value: str) -> int:
    return int(value, 0)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path, help="user-supplied decrypted ELF64 big-endian PPU image")
    parser.add_argument("--title-id", required=True, choices=sorted(TITLE_IDS))
    parser.add_argument("--title-update", default="1.16")
    parser.add_argument("--firmware-family", default="CEX", choices=("CEX",))
    parser.add_argument("--firmware-version", default="4.93")
    parser.add_argument("--image-base", type=_parse_int, help="optional base to test reported offsets as RVAs; never guessed")
    parser.add_argument("--max-bytes", type=int, default=32, help="maximum bytes to include at a mapped candidate (4..256)")
    parser.add_argument("--output", type=Path, help="optional JSON report path; defaults to stdout")
    args = parser.parse_args(argv)

    try:
        report = inspect_file(
            args.elf, args.title_id, args.title_update, args.firmware_family,
            args.firmware_version, args.image_base, args.max_bytes,
        )
    except (OSError, ValueError, ElfFormatError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    rendered = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(rendered, encoding="utf-8")
        print(f"wrote read-only analysis report: {args.output}")
    else:
        sys.stdout.write(rendered)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
