import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from ps3_zone_pool_inspect import PoolError,inspect_pool

def make_zone(tag_count,records,trailer=b""):
    zone=bytearray(0x38)
    struct.pack_into(">I",zone,0x28,tag_count)
    struct.pack_into(">I",zone,0x30,len(records))
    if tag_count:
        zone.extend(b"\0"*4)
        zone.extend(struct.pack(">i",-1)*tag_count)
        zone.extend(b"tag0\0")
        zone.extend(trailer)
    for pointer,type_id in records:
        zone.extend(struct.pack(">II",pointer,type_id))
    return bytes(zone)

class Ps3ZonePoolInspectTests(unittest.TestCase):
    def test_zero_tags_pool_starts_at_0x38(self):
        zone=make_zone(0,[(0xFFFFFFFF,0x29),(0xFFFFFFFF,0x28)])
        report=inspect_pool(zone)
        self.assertEqual(report["pool_offset"],0x38)
        self.assertEqual(report["scriptfile_count_type_0x29"],1)
        self.assertEqual(report["rawfile_count_type_0x28"],1)

    def test_three_byte_variable_trailer_and_mapents(self):
        zone=make_zone(1,[(0xFFFFFFFF,0x14),(0xFFFFFFFF,0x29)],b"\xAA\xBB\xCC")
        report=inspect_pool(zone)
        self.assertEqual(report["pool_trailer_bytes"],3)
        self.assertEqual(report["map_ents_count_type_0x14"],1)
        self.assertEqual(report["map_ents_entries"][0]["pointer"],"0xFFFFFFFF")

    def test_four_byte_variable_trailer(self):
        zone=make_zone(1,[(0xFFFFFFFF,0x28),(0xFFFFFFFF,0x29)],b"\0\0\0\0")
        report=inspect_pool(zone)
        self.assertEqual(report["pool_trailer_bytes"],4)
        self.assertEqual(report["valid_record_count"],2)

    def test_bad_header_rejected(self):
        with self.assertRaises(PoolError):
            inspect_pool(b"short")

if __name__=="__main__":
    unittest.main()
