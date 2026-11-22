import struct
import unittest

from tools.ps3_mapents_inspect import find_mapents


def fixture_mapents(offset=0x103):
    entities = (
        b'{\n97 "worldspawn"\n642 "clockwork"\n}\n'
        b'{\n97 "trigger_multiple"\n642 "drill_spot1"\n430 "1 2 3"\n}\n'
    ) + b"\0"
    data = bytearray(offset + 0x80 + len(entities) + 64)
    struct.pack_into(">I", data, offset + 0x04, 0xFFFFFFFF)
    struct.pack_into(">i", data, offset + 0x08, len(entities))
    struct.pack_into(">H", data, offset + 0x70, 7)
    data[offset + 0x80:offset + 0x80 + len(entities)] = entities
    return bytes(data), entities[:-1]


class MapEntsTests(unittest.TestCase):
    def test_finds_unaligned_ps3_mapents_and_parses_targetnames(self):
        data, expected = fixture_mapents()
        found = find_mapents(data)
        self.assertEqual(len(found), 1)
        candidate, entity_text, entities = found[0]
        self.assertEqual(candidate.header_offset, 0x103)
        self.assertEqual(candidate.spawn_count, 7)
        self.assertEqual(entity_text, expected)
        self.assertEqual(entities[1]["97"], "trigger_multiple")
        self.assertEqual(entities[1]["642"], "drill_spot1")

    def test_rejects_incorrect_entity_string_length(self):
        data, _ = fixture_mapents()
        damaged = bytearray(data)
        struct.pack_into(">i", damaged, 0x103 + 0x08, 100000)
        self.assertEqual(find_mapents(bytes(damaged)), [])

    def test_rejects_non_inline_entity_string_pointer(self):
        data, _ = fixture_mapents()
        damaged = bytearray(data)
        struct.pack_into(">I", damaged, 0x103 + 0x04, 0x12345678)
        self.assertEqual(find_mapents(bytes(damaged)), [])

    def test_explicit_offset_rejects_wrong_header(self):
        data, _ = fixture_mapents()
        self.assertEqual(find_mapents(data, 0x200), [])

    def test_ambiguous_candidates_are_not_silently_selected(self):
        first, _ = fixture_mapents(0x103)
        second, _ = fixture_mapents(0x503)
        merged = bytearray(max(len(first), len(second)))
        merged[:len(first)] = first
        size = 0x80 + len(fixture_mapents(0x503)[1]) + 1 + 64
        merged[0x503:0x503 + size] = second[0x503:0x503 + size]
        self.assertEqual(len(find_mapents(bytes(merged))), 2)


if __name__ == "__main__":
    unittest.main()
