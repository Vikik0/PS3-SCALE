import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from ps3_zone_mapents_probe import find_mapents,parse_script_strings

def make_zone():
    zone=bytearray(0x3000)
    count=3
    struct.pack_into(">i",zone,0x28,count)
    struct.pack_into(">iii",zone,0x3c,0,-1,-1)
    strings=b"mp_dm_spawn\0target_a\0"
    strings_offset=0x3c+4*count
    zone[strings_offset:strings_offset+len(strings)]=strings
    mapents=0x1000
    entity_offset=mapents+128
    blocks=[]
    for index in range(6):
        classname="info_player_start" if index==0 else "script_model"
        blocks.append("{\n97 \""+classname+"\"\n430 \"0 0 0\"\n47 \"0 0 0\"\n}")
    entity_text=("\n".join(blocks)).encode()+b"\0"
    struct.pack_into(">i",zone,mapents+8,len(entity_text))
    spawn_count=1
    spawn_table=entity_offset+len(entity_text)
    struct.pack_into(">H",zone,mapents+112,spawn_count)
    struct.pack_into(">i",zone,mapents+116,spawn_table)
    zone[entity_offset:entity_offset+len(entity_text)]=entity_text
    struct.pack_into(">HHHHffffff",zone,spawn_table,0,1,2,0,100.0,200.0,300.0,0.0,90.0,0.0)
    return bytes(zone)

class Ps3ZoneMapEntsProbeTests(unittest.TestCase):
    def test_script_string_pointer_slots(self):
        strings=parse_script_strings(make_zone())
        self.assertEqual(strings["count"],3)
        self.assertEqual(strings["strings"],["","mp_dm_spawn","target_a"])

    def test_mapents_entity_text_and_spawn_list(self):
        zone=make_zone()
        candidates=find_mapents(zone,parse_script_strings(zone))
        self.assertEqual(len(candidates),1)
        candidate=candidates[0]
        self.assertEqual(candidate["mapents_header_offset"],0x1000)
        self.assertEqual(candidate["entity_block_count"],6)
        self.assertEqual(candidate["spawn_count"],1)
        self.assertTrue(candidate["spawn_table_found"])
        self.assertEqual(candidate["spawn_records"][0]["classname"],"mp_dm_spawn")
        self.assertEqual(candidate["spawn_records"][0]["origin"],[100.0,200.0,300.0])

if __name__=="__main__":
    unittest.main()
