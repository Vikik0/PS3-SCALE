import struct
import tempfile
import unittest
from pathlib import Path
from tools.ps3_sfo_inspect import SfoFormatError, inspect_file, parse_sfo

def make_sfo(entries):
    key_table, data_table, index_entries = bytearray(), bytearray(), []
    for key, fmt, value in entries:
        key_offset = len(key_table)
        key_table.extend(key.encode("ascii") + b"\x00")
        if fmt == 0x0204:
            raw = value.encode("utf-8") + b"\x00"
            max_len = len(raw)
        elif fmt == 0x0404:
            raw, max_len = struct.pack("<I", value), 4
        else:
            raw = bytes(value)
            max_len = len(raw)
        value_offset = len(data_table)
        data_table.extend(raw)
        index_entries.append((key_offset, fmt, len(raw), max_len, value_offset))
    key_start = 20 + len(index_entries) * 16
    data_start = key_start + len(key_table)
    header = struct.pack("<4sIIII", b"\x00PSF", 0x00000101, key_start, data_start, len(index_entries))
    index = b"".join(struct.pack("<HHIII", *item) for item in index_entries)
    return header + index + bytes(key_table) + bytes(data_table)

class Ps3SfoInspectTests(unittest.TestCase):
    def test_reads_selected_ghosts_title_id(self):
        report = parse_sfo(make_sfo([
            ("TITLE_ID", 0x0204, "BLUS31270"),
            ("TITLE", 0x0204, "Call of Duty: Ghosts"),
            ("APP_VER", 0x0204, "01.00"),
            ("VERSION", 0x0204, "01.00"),
            ("ATTRIBUTE", 0x0404, 0),
        ]))
        self.assertEqual(report["title_id"], "BLUS31270")
        self.assertTrue(report["title_id_known_ghosts"])
        self.assertTrue(report["selected_target_match"])
        self.assertEqual(report["title"], "Call of Duty: Ghosts")
        self.assertEqual(report["app_version"], "01.00")
        self.assertEqual(report["entries"]["ATTRIBUTE"], 0)

    def test_other_known_region_is_not_selected_target(self):
        report = parse_sfo(make_sfo([("TITLE_ID", 0x0204, "BLES01945")]))
        self.assertTrue(report["title_id_known_ghosts"])
        self.assertFalse(report["selected_target_match"])

    def test_inspect_file_reports_hash_and_size(self):
        fixture = make_sfo([("TITLE_ID", 0x0204, "BLUS31270")])
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "PARAM.SFO"
            path.write_bytes(fixture)
            report = inspect_file(path)
        self.assertEqual(report["input"]["name"], "PARAM.SFO")
        self.assertEqual(report["input"]["size_bytes"], len(fixture))
        self.assertEqual(len(report["input"]["sha256"]), 64)
        self.assertIn("does not prove", report["warning"])

    def test_rejects_wrong_magic(self):
        with self.assertRaisesRegex(SfoFormatError, "magic"):
            parse_sfo(b"NOPE" + bytes(64))

    def test_rejects_truncated_header(self):
        with self.assertRaisesRegex(SfoFormatError, "shorter"):
            parse_sfo(b"\x00PSF")

    def test_rejects_out_of_bounds_key_table(self):
        data = bytearray(make_sfo([("TITLE_ID", 0x0204, "BLUS31270")]))
        struct.pack_into("<I", data, 8, 0xFFFF)
        with self.assertRaises(SfoFormatError):
            parse_sfo(bytes(data))

    def test_rejects_non_terminated_key(self):
        data = bytearray(make_sfo([("TITLE_ID", 0x0204, "BLUS31270")]))
        key_start = struct.unpack_from("<I", data, 8)[0]
        key_end = struct.unpack_from("<I", data, 12)[0]
        data[key_start:key_end] = b"TITLE_ID"
        with self.assertRaisesRegex(SfoFormatError, "NUL-terminated"):
            parse_sfo(bytes(data))

    def test_rejects_integer_with_wrong_length(self):
        data = bytearray(make_sfo([("BAD_INT", 0x0404, 7)]))
        struct.pack_into("<I", data, 20 + 4, 3)
        with self.assertRaisesRegex(SfoFormatError, "integer is not 4 bytes"):
            parse_sfo(bytes(data))

if __name__ == "__main__":
    unittest.main()
