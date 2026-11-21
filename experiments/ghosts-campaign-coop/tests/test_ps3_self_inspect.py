import importlib.util
import struct
import unittest
from pathlib import Path

TOOL = Path(__file__).parents[1] / "tools" / "ps3_self_inspect.py"
SPEC = importlib.util.spec_from_file_location("ps3_self_inspect", TOOL)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)

def make_self() -> bytes:
    data = bytearray(0x1200)
    MODULE.SCE_HEADER.pack_into(data, 0, b"SCE\\x00", 2, 0x1C, 1, 0x4A0, 0xA80, 0x20000)
    MODULE.EXT_HEADER.pack_into(data, 0x20, 3, 0x70, 0x90, 0xD0, 0, 0x290, 0x390, 0x3C0, 0x70, 0)
    MODULE.APP_INFO.pack_into(data, 0x70, 0x1010000001000003, 0x01000002, 8, 0x0001000000000000, 0)
    ident = b"\\x7fELF" + bytes([2, 2, 1, 0x66]) + bytes(8)
    MODULE.ELF_HEADER.pack_into(data, 0x90, ident, 2, 21, 1, 0x10000, 64, 0, 0, 64, 56, 1, 64, 0, 0)
    MODULE.ELF64_PHDR.pack_into(data, 0xD0, 1, 5, 0, 0x10000, 0x10000, 0x100, 0x100, 0x1000)
    MODULE.SEGMENT_INFO.pack_into(data, 0x290, 0xA80, 0x100, 0x200000000, 1)
    return bytes(data)

class SelfInspectorTests(unittest.TestCase):
    def test_reads_ps3_ppc64_self_header(self):
        report = MODULE.inspect_self(make_self(), "fixture.self")
        self.assertEqual(report["container"]["key_revision"], 0x1C)
        self.assertEqual(report["embedded_elf_header"]["machine"], "PowerPC64")
        self.assertEqual(report["embedded_elf_header"]["entry_point"], "0x10000")
        self.assertEqual(report["segment_summary"]["encrypted_segments"], 1)
        self.assertTrue(report["segment_summary"]["all_container_ranges_valid"])

    def test_rejects_non_self(self):
        with self.assertRaises(MODULE.SelfFormatError):
            MODULE.inspect_self(b"NOTSELF" + bytes(100))

    def test_rejects_truncated_header(self):
        with self.assertRaises(MODULE.SelfFormatError):
            MODULE.inspect_self(b"SCE\\x00")

    def test_rejects_out_of_range_segment(self):
        data = bytearray(make_self())
        MODULE.SEGMENT_INFO.pack_into(data, 0x290, 0x11F0, 0x100, 0x200000000, 1)
        with self.assertRaises(MODULE.SelfFormatError):
            MODULE.inspect_self(bytes(data))

    def test_rejects_wrong_machine(self):
        data = bytearray(make_self())
        struct.pack_into(">H", data, 0x90 + 18, 3)
        with self.assertRaises(MODULE.SelfFormatError):
            MODULE.inspect_self(bytes(data))

if __name__ == "__main__":
    unittest.main()
