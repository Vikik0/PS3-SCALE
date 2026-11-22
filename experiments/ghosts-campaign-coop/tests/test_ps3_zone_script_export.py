import struct
import tempfile
import unittest
import zlib
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from ps3_zone_script_export import ZoneScanError, export_zone, scan_zone

def make_long_asset(name: str, stack: bytes, bytecode: bytes) -> bytes:
    compressed = zlib.compress(stack)
    header = (
        b"\xff" * 4
        + struct.pack(">III", len(compressed), len(stack), len(bytecode))
        + b"\xff" * 8
        + name.encode("ascii")
        + b"\x00"
    )
    return header + compressed + bytecode

def make_short_asset(name: str, body: bytes) -> bytes:
    compressed = zlib.compress(body)
    header = (
        b"\xff" * 4
        + struct.pack(">II", len(compressed), len(body))
        + b"\xff" * 4
        + name.encode("ascii")
        + b"\x00"
    )
    return header + compressed

class Ps3ZoneScriptExportTests(unittest.TestCase):
    def test_long_asset_rebuilt_as_gsc_tool_wrapper(self):
        stack = b"stack-buffer"
        bytecode = b"\x01\x02\x03"
        zone = bytes(0x40) + make_long_asset("maps/mp/test", stack, bytecode)
        assets = scan_zone(zone)
        self.assertEqual(len(assets), 1)
        record, wrapper = assets[0]
        self.assertEqual(record.kind, "compiled_gsc_candidate")
        self.assertEqual(record.output_name, "maps/mp/test.gscbin")
        self.assertEqual(wrapper[:4], b"GSC\x00")
        compressed_len, stack_len, bytecode_len = struct.unpack_from("<III", wrapper, 4)
        self.assertEqual(stack_len, len(stack))
        self.assertEqual(bytecode_len, len(bytecode))
        self.assertEqual(zlib.decompress(wrapper[16:16 + compressed_len]), stack)
        self.assertEqual(wrapper[16 + compressed_len:], bytecode)

    def test_short_asset_exports_decompressed_rawfile(self):
        body = b"raw-data"
        zone = bytes(0x40) + make_short_asset("vision/test.vision", body)
        assets = scan_zone(zone)
        self.assertEqual(len(assets), 1)
        self.assertEqual(assets[0][0].kind, "rawfile")
        self.assertEqual(assets[0][1], body)

    def test_short_gsc_source_exports_plain_source(self):
        body = b"main() { return; }"
        zone = bytes(0x40) + make_short_asset("maps/test.gsc", body)
        assets = scan_zone(zone)
        self.assertEqual(assets[0][0].kind, "gsc_source")
        self.assertEqual(assets[0][1], body)

    def test_invalid_lengths_and_traversal_are_ignored(self):
        self.assertEqual(scan_zone(bytes(0x40) + make_long_asset("../escape", b"stack", b"x")), [])
        compressed = zlib.compress(b"bad")
        malformed = (
            b"\xff" * 4 + struct.pack(">III", len(compressed), 999, 1)
            + b"\xff" * 8 + b"maps/x\x00" + compressed + b"x"
        )
        self.assertEqual(scan_zone(bytes(0x40) + malformed), [])

    def test_export_creates_gscbin_and_rawfile_directories(self):
        zone = (
            bytes(0x40)
            + make_long_asset("maps/mp/example", b"stack", b"\x01\x02")
            + make_short_asset("raw/example.bin", b"raw")
        )
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            zone_path = root / "example.raw.zone"
            output = root / "out"
            zone_path.write_bytes(zone)
            report = export_zone(zone_path, output)
            self.assertEqual(report["compiled_gsc_candidates"], 1)
            self.assertEqual(report["rawfile_count"], 1)
            self.assertEqual((output / "gscbin" / "maps" / "mp" / "example.gscbin").read_bytes()[:4], b"GSC\x00")
            self.assertEqual((output / "rawfiles" / "raw" / "example.bin").read_bytes(), b"raw")

    def test_tiny_zone_rejected(self):
        with self.assertRaises(ZoneScanError):
            scan_zone(b"tiny")

if __name__ == "__main__":
    unittest.main()
