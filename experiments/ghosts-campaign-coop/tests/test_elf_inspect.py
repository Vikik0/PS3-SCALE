import struct
import tempfile
import unittest
from pathlib import Path

from tools.ps3_elf_inspect import ElfFormatError, PpuElf, inspect_file


def make_ppc64_elf(*, machine=21, data_encoding=2, segment_flags=5):
    """Minimal ELF64 BE PPU image with one PT_LOAD at VA 0x1000/file 0x100."""
    ident = bytearray(b"\x7fELF" + bytes([2, data_encoding, 1, 0, 0]) + bytes(7))
    header = struct.pack(
        ">16sHHIQQQIHHHHHH", bytes(ident), 2, machine, 1,
        0x1000, 64, 0, 0, 64, 56, 1, 64, 0, 0,
    )
    program_header = struct.pack(
        ">IIQQQQQQ", 1, segment_flags, 0x100, 0x1000,
        0x1000, 0x80, 0x80, 0x1000,
    )
    data = bytearray(0x180)
    data[:64] = header
    data[64:120] = program_header
    data[0x100:0x180] = bytes(range(0x80))
    return bytes(data)


def make_ppc64_elf_with_bss():
    ident = bytearray(b"\x7fELF" + bytes([2, 2, 1, 0, 0]) + bytes(7))
    header = struct.pack(
        ">16sHHIQQQIHHHHHH", bytes(ident), 2, 21, 1,
        0x1000, 64, 0, 0, 64, 56, 1, 64, 0, 0,
    )
    program_header = struct.pack(
        ">IIQQQQQQ", 1, 6, 0x100, 0x1000,
        0x1000, 0x80, 0x100, 0x1000,
    )
    data = bytearray(0x180)
    data[:64] = header
    data[64:120] = program_header
    data[0x100:0x180] = bytes(range(0x80))
    return bytes(data)


def make_ppc64_elf_with_opd():
    """ELF with executable code plus a data segment holding two OPD entries."""
    ident = bytearray(b"\x7fELF" + bytes([2, 2, 1, 0, 0]) + bytes(7))
    header = struct.pack(
        ">16sHHIQQQIHHHHHH", bytes(ident), 2, 21, 1,
        0x2000, 64, 0x300, 0, 64, 56, 2, 64, 2, 0,
    )
    code_segment = struct.pack(
        ">IIQQQQQQ", 1, 5, 0x100, 0x1000,
        0x1000, 0x80, 0x80, 0x1000,
    )
    data_segment = struct.pack(
        ">IIQQQQQQ", 1, 4, 0x200, 0x2000,
        0x2000, 0x10, 0x10, 8,
    )
    null_section = bytes(64)
    opd_section = struct.pack(
        ">IIQQQQIIQQ", 0, 1, 0, 0x2000, 0x200, 0x10, 0, 0, 8, 8,
    )
    data = bytearray(0x380)
    data[:64] = header
    data[64:120] = code_segment
    data[120:176] = data_segment
    data[0x100:0x180] = bytes(range(0x80))
    data[0x200:0x210] = struct.pack(">IIII", 0x1010, 0x3000, 0x1020, 0x3000)
    data[0x300:0x340] = null_section
    data[0x340:0x380] = opd_section
    return bytes(data)


class PpuElfTests(unittest.TestCase):
    def test_rejects_non_ppc64_elf(self):
        with self.assertRaisesRegex(ElfFormatError, "EM_PPC64"):
            PpuElf(make_ppc64_elf(machine=3))

    def test_rejects_little_endian(self):
        with self.assertRaisesRegex(ElfFormatError, "big-endian"):
            PpuElf(make_ppc64_elf(data_encoding=1))

    def test_maps_virtual_address_to_file_offset(self):
        elf = PpuElf(make_ppc64_elf())
        mapping = elf.map_virtual_address(0x1010)
        self.assertIsNotNone(mapping)
        self.assertEqual(mapping.file_offset, 0x110)
        self.assertTrue(mapping.executable)

    def test_does_not_map_zero_fill_tail_as_file_bytes(self):
        elf = PpuElf(make_ppc64_elf())
        self.assertIsNone(elf.map_virtual_address(0x1080))

    def test_marks_non_executable_segment(self):
        elf = PpuElf(make_ppc64_elf(segment_flags=6))
        mapping = elf.map_virtual_address(0x1010)
        self.assertFalse(mapping.executable)
        self.assertEqual(mapping.permissions, "rw-")

    def test_rejects_out_of_bounds_program_header(self):
        data = bytearray(make_ppc64_elf())
        struct.pack_into(">Q", data, 32, 0x10000)
        with self.assertRaisesRegex(ElfFormatError, "program-header table"):
            PpuElf(bytes(data))

    def test_maps_zero_fill_bss_without_inventing_file_bytes(self):
        elf = PpuElf(make_ppc64_elf_with_bss())
        mapping = elf.map_memory_address(0x1090)
        self.assertIsNotNone(mapping)
        self.assertFalse(mapping.file_backed)
        self.assertIsNone(mapping.file_offset)
        self.assertIsNone(elf.map_virtual_address(0x1090))

    def test_reads_ps3_opd_entry_and_module_toc(self):
        elf = PpuElf(make_ppc64_elf_with_opd())
        info = elf.opd_info()
        self.assertEqual(info["descriptor_count"], 2)
        self.assertEqual(info["entry_descriptor"]["code_address"], "0x1010")
        self.assertEqual(info["entry_descriptor"]["toc"], "0x3000")
        self.assertEqual(info["module_toc"], "0x3000")

    def test_inspection_reports_hash_and_operator_asserted_profile(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "decrypted-eboot.elf"
            path.write_bytes(make_ppc64_elf())
            report = inspect_file(path, "BLUS31270", image_base=0x1000)
        self.assertEqual(report["input"]["size_bytes"], 0x180)
        self.assertEqual(len(report["input"]["sha256"]), 64)
        self.assertEqual(report["operator_asserted_profile"]["title_id"], "BLUS31270")
        self.assertEqual(report["operator_asserted_profile"]["title_update"], "1.16")
        self.assertIn("warning", report["operator_asserted_profile"])
        self.assertEqual(len(report["candidate_offsets"]), 16)
        self.assertEqual(len(report["reported_structure_hints"]), 2)

    def test_rejects_unregistered_title_id(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "decrypted-eboot.elf"
            path.write_bytes(make_ppc64_elf())
            with self.assertRaisesRegex(ValueError, "Title ID"):
                inspect_file(path, "BLUS00000")


if __name__ == "__main__":
    unittest.main()
