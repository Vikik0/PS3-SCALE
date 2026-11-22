import struct
import tempfile
import unittest
import zlib
from pathlib import Path

from tools.ps3mapi.scan_clockwork_script_asset import (
    ScriptAssetError,
    analyze,
    build_scriptfile_asset,
    find_script_assets,
    parse_gscbin,
)


def make_gscbin(stack: bytes, bytecode: bytes) -> bytes:
    compressed = zlib.compress(stack)
    return (
        b"GSC\0"
        + struct.pack("<III", len(compressed), len(stack), len(bytecode))
        + compressed
        + bytecode
    )


class ClockworkScriptAssetScannerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def test_gscbin_wrapper_is_parsed_and_stack_is_validated(self):
        data = make_gscbin(b"clockwork stack data", b"\x60\x00\x00\x00" * 8)
        parsed = parse_gscbin(data)
        self.assertEqual(parsed["stack"], b"clockwork stack data")
        self.assertEqual(parsed["bytecode_len"], 32)
        self.assertEqual(parsed["wrapper_bytes"], len(data))

    def test_replacement_asset_uses_fastfile_big_endian_header(self):
        data = make_gscbin(b"stack", b"\x01\x02\x03\x04")
        asset, report = build_scriptfile_asset(data)
        self.assertEqual(asset[:4], b"\xff" * 4)
        self.assertEqual(struct.unpack_from(">III", asset, 4), (
            report["compressed_len"], report["stack_len"], report["bytecode_len"]
        ))
        self.assertEqual(asset[16:24], b"\xff" * 8)
        self.assertTrue(asset.endswith(b"\x01\x02\x03\x04"))

    def test_memory_dump_scanner_finds_and_validates_scriptfile(self):
        gscbin = make_gscbin(b"compressed stack fixture", b"\xaa\xbb\xcc\xdd" * 16)
        asset, _ = build_scriptfile_asset(gscbin)
        blob = b"prefix-data" + asset + b"suffix-data"
        matches = find_script_assets(blob, base_address=0x10000000)
        self.assertEqual(len(matches), 1)
        self.assertEqual(matches[0]["header_offset"], len(b"prefix-data"))
        self.assertEqual(matches[0]["header_address"], f"0x{0x10000000 + len(b'prefix-data'):X}")
        self.assertEqual(matches[0]["asset_bytes"], len(asset))

    def test_invalid_asset_header_is_ignored(self):
        gscbin = make_gscbin(b"stack", b"\x11\x22\x33\x44")
        asset, _ = build_scriptfile_asset(gscbin)
        damaged = bytearray(asset)
        damaged[16] = 0x00
        self.assertEqual(find_script_assets(b"prefix" + bytes(damaged)), [])

    def test_invalid_gscbin_length_is_rejected(self):
        data = make_gscbin(b"stack", b"\x11\x22\x33\x44") + b"trailing"
        with self.assertRaisesRegex(ScriptAssetError, "length mismatch"):
            parse_gscbin(data)

    def test_invalid_compressed_stack_is_rejected(self):
        data = b"GSC\0" + struct.pack("<III", 3, 100, 4) + b"bad" + b"\x00" * 4
        with self.assertRaisesRegex(ScriptAssetError, "compressed GSC stack"):
            parse_gscbin(data)

    def test_analyze_reports_runtime_growth_when_replacement_is_larger(self):
        original_gscbin = make_gscbin(b"small stack", b"\x00" * 8)
        original_asset, _ = build_scriptfile_asset(original_gscbin)
        new_gscbin = make_gscbin(b"large stack " * 100, b"\x11" * 1200)
        raw_zone = self.root / "clockwork.raw.zone"
        gscbin_path = self.root / "patched.gscbin"
        raw_zone.write_bytes(b"zone-prefix" + original_asset + b"zone-tail")
        gscbin_path.write_bytes(new_gscbin)

        report, replacement = analyze(
            raw_zone=raw_zone,
            memory_dump=None,
            gscbin=gscbin_path,
        )
        self.assertEqual(report["status"], "SCRIPT_ASSET_FOUND_REPLACEMENT_BLOB_BUILT")
        self.assertIsNotNone(replacement)
        self.assertGreater(report["size_comparison"]["delta_bytes"], 0)
        self.assertFalse(report["size_comparison"]["in_place_replacement_fits"])
        self.assertTrue(report["size_comparison"]["runtime_relocation_or_memory_growth_required"])

    def test_missing_script_asset_is_reported(self):
        raw_zone = self.root / "empty.zone"
        raw_zone.write_bytes(b"no ScriptFile here")
        report, replacement = analyze(
            raw_zone=raw_zone,
            memory_dump=None,
            gscbin=None,
        )
        self.assertEqual(report["status"], "SCRIPT_ASSET_NOT_FOUND")
        self.assertIsNone(replacement)

    def test_scan_requires_at_least_one_input(self):
        with self.assertRaisesRegex(ValueError, "provide at least one"):
            analyze(raw_zone=None, memory_dump=None, gscbin=None)


if __name__ == "__main__":
    unittest.main()
