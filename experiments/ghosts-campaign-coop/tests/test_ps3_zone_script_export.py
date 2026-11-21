import struct
import tempfile
import unittest
import zlib
from pathlib import Path

from tools.ps3_zone_script_export import ZoneScanError, export_zone, scan_zone


def make_asset(name: str, body: bytes, *, long_header: bool) -> bytes:
    comp = zlib.compress(body)
    header = b"\xff" * 4 + struct.pack(">II", len(comp), len(body))
    if long_header:
        header += struct.pack(">I", 0x12345678) + b"\xff" * 8
    else:
        header += b"\xff" * 4
    return header + name.encode("ascii") + b"\x00" + body


class Ps3ZoneScriptExportTests(unittest.TestCase):
    def test_exports_short_gsc_source(self):
        body = b"main() {\n    level endon(\"disconnect\");\n}\n"
        zone = bytes(0x40) + make_asset("maps/mp/test.gsc", body, long_header=False)
        assets = scan_zone(zone)
        self.assertEqual(len(assets), 1)
        self.assertEqual(assets[0][0].kind, "gsc_source")
        self.assertEqual(assets[0][1], body)

    def test_exports_long_compiled_gsc_as_gscbin(self):
        body = b"GSC\x00" + struct.pack("<III", 4, 4, 0) + b"TEST"
        zone = bytes(0x40) + make_asset("maps/mp/test.gsc", body, long_header=True)
        assets = scan_zone(zone)
        self.assertEqual(len(assets), 1)
        self.assertEqual(assets[0][0].kind, "compiled_gsc")
        self.assertEqual(assets[0][0].output_name, "maps/mp/test.gscbin")
        self.assertEqual(assets[0][1], body)

    def test_ignores_non_script_assets(self):
        body = b"plain vision data"
        zone = bytes(0x40) + make_asset("vision/mp_test.vision", body, long_header=False)
        self.assertEqual(scan_zone(zone), [])

    def test_rejects_path_traversal(self):
        body = b"GSC\x00script"
        zone = bytes(0x40) + make_asset("../escape.gsc", body, long_header=True)
        self.assertEqual(scan_zone(zone), [])

    def test_export_creates_manifestable_output(self):
        body = b"GSC\x00compiled script"
        zone = bytes(0x40) + make_asset("maps/mp/example.gsc", body, long_header=True)
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            zone_path = root / "example.zone"
            out = root / "out"
            zone_path.write_bytes(zone)
            report = export_zone(zone_path, out)
            self.assertEqual(report["script_asset_count"], 1)
            exported = out / "maps" / "mp" / "example.gscbin"
            self.assertEqual(exported.read_bytes(), body)

    def test_tiny_zone_rejected(self):
        with self.assertRaises(ZoneScanError):
            scan_zone(b"tiny")


if __name__ == "__main__":
    unittest.main()
