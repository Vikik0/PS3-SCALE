import io
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import extract_disc


class PSXIndexTests(unittest.TestCase):
    def test_reads_dat_table_from_psx_exe(self):
        exe = bytearray(0x800 + 0x100)
        exe[:8] = b"PS-X EXE"
        struct.pack_into("<I", exe, 0x18, 0x80010000)
        table_addr = 0x80010020
        table_offset = 0x800 + table_addr - 0x80010000
        struct.pack_into("<4I", exe, table_offset, 3, 100, 8, 2048)
        entries, offset, text_addr = extract_disc.psx_dat_index(bytes(exe), table_addr, 2)
        self.assertEqual(entries, [(3, 100), (8, 2048)])
        self.assertEqual(offset, table_offset)
        self.assertEqual(text_addr, 0x80010000)

    def test_rejects_wrong_executable(self):
        with self.assertRaises(ValueError):
            extract_disc.psx_dat_index(b"not an exe")


class DATSplitTests(unittest.TestCase):
    def test_round_trip_preserves_unindexed_padding(self):
        data = bytes((i * 13) & 255 for i in range(5 * 2048))
        entries = [(0, 2148), (3, 50), (4, 2048)]
        files, filler, gaps = extract_disc.split_dat(data, entries)
        self.assertEqual([len(x) for x in files], [2148, 50, 2048])
        self.assertEqual(len(filler), sum(b - a for a, b in gaps))
        self.assertEqual(sum(map(len, files)) + len(filler), len(data))

    def test_rejects_overlapping_entries(self):
        with self.assertRaises(ValueError):
            extract_disc.dat_gaps([(0, 2049), (1, 100)], 4096)

    def test_rejects_entries_outside_archive(self):
        with self.assertRaises(ValueError):
            extract_disc.dat_gaps([(2, 100)], 4096)


class ISOTests(unittest.TestCase):
    def make_sector(self, submode=0x08):
        raw = bytearray(2352)
        raw[:12] = b"\x00" + b"\xff" * 10 + b"\x00"
        raw[15] = 2
        sub = bytes((0, 0, submode, 0))
        raw[16:20] = sub
        raw[20:24] = sub
        raw[24:24 + 2048] = b"A" * 2048
        return bytes(raw)

    def test_reads_mode2_form1_payload(self):
        payload = extract_disc.sector_payload(io.BytesIO(self.make_sector()), 0)
        self.assertEqual(payload, b"A" * 2048)

    def test_rejects_form2_payload(self):
        with self.assertRaises(ValueError):
            extract_disc.sector_payload(io.BytesIO(self.make_sector(0x20)), 0)


if __name__ == "__main__":
    unittest.main()
