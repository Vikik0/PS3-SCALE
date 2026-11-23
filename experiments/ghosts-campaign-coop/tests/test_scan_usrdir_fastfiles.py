import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from scan_usrdir_fastfiles import UsrdirScanError, scan_usrdir
from ps3_fastfile_inspect import INNER_MAGIC, OUTER_MAGIC, STREAM_DELTA, ZONE_BLOCK_SIZE


def _raw_deflate(data):
    compressor = zlib.compressobj(level=9, wbits=-15)
    return compressor.compress(data) + compressor.flush()


def _make_fastfile(zone):
    marker_offset = 0x24
    stream_offset = marker_offset + STREAM_DELTA
    payload = _raw_deflate(zone) + b"\x12\x34\x56\x78"
    data = bytearray(stream_offset)
    data[:8] = OUTER_MAGIC
    struct.pack_into(">I", data, 8, 0x22E)
    data[marker_offset:marker_offset + len(INNER_MAGIC)] = INNER_MAGIC
    data.extend(struct.pack(">H", len(payload)))
    data.extend(payload)
    data.extend(b"\x00\x01\x00\x00\x00\x00")
    return bytes(data)


def _make_zone():
    zone = bytearray(ZONE_BLOCK_SIZE)
    struct.pack_into(">I", zone, 0x28, 0)
    struct.pack_into(">I", zone, 0x30, 1)
    struct.pack_into(">II", zone, 0x38, 0xFFFFFFFF, 0x14)
    path = b"maps/clockwork.d3dbsp\x00"
    zone[0x100:0x100 + len(path)] = path
    return bytes(zone)


class ScanUsrdirFastFilesTests(unittest.TestCase):
    def test_recursively_inventories_fastfiles_and_map_references(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp) / "BLUS31270" / "PS3_GAME" / "USRDIR"
            nested = root / "english"
            nested.mkdir(parents=True)
            (root / "clockwork.ff").write_bytes(_make_fastfile(_make_zone()))
            (nested / "black_ice.ff").write_bytes(_make_fastfile(_make_zone()))
            (root / "clockwork_load.bik").write_bytes(b"not-in-inventory")
            report = scan_usrdir(root)
            self.assertEqual(report["fastfile_count"], 2)
            self.assertEqual(report["fastfiles_parsed"], 2)
            self.assertEqual(report["mapents_candidate_count"], 0)
            clockwork = next(row for row in report["fastfiles"] if row["name"] == "clockwork.ff")
            self.assertEqual(clockwork["asset_pool"]["map_ents_count"], 1)
            self.assertEqual(clockwork["map_path_references"], ["maps/clockwork.d3dbsp"])

    def test_invalid_fastfile_is_reported_without_aborting_directory_scan(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "good.ff").write_bytes(_make_fastfile(_make_zone()))
            (root / "bad.ff").write_bytes(b"not-a-ghosts-fastfile")
            report = scan_usrdir(root)
            self.assertEqual(report["fastfile_count"], 2)
            self.assertEqual(report["fastfiles_parsed"], 1)
            self.assertEqual(report["fastfiles_unsupported_or_invalid"], 1)
            bad = next(row for row in report["fastfiles"] if row["name"] == "bad.ff")
            self.assertEqual(bad["status"], "UNSUPPORTED_OR_INVALID_FASTFILE")

    def test_rejects_non_directory_and_nonpositive_file_limit(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            not_a_dir = root / "file.ff"
            not_a_dir.write_bytes(b"x")
            with self.assertRaisesRegex(UsrdirScanError, "not a directory"):
                scan_usrdir(not_a_dir)
            with self.assertRaisesRegex(UsrdirScanError, "must be positive"):
                scan_usrdir(root, max_files=0)

    def test_rejects_directory_over_file_limit(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "a.ff").write_bytes(b"x")
            (root / "b.ff").write_bytes(b"x")
            with self.assertRaisesRegex(UsrdirScanError, "above --max-files"):
                scan_usrdir(root, max_files=1)


if __name__ == "__main__":
    unittest.main()
