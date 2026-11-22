import struct
import unittest
import zlib
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from ps3_fastfile_inspect import (
    FastFileError, INNER_MAGIC, OUTER_MAGIC, STREAM_DELTA, ZONE_BLOCK_SIZE,
    inspect_bytes, inspect_bytes_full,
)

def raw_deflate(data: bytes) -> bytes:
    compressor = zlib.compressobj(level=9, wbits=-15)
    return compressor.compress(data) + compressor.flush()

def make_fastfile(zone: bytes, *, marker_offset: int = 0x24,
                  block_trailer: bytes = b"\x12\x34\x56\x78",
                  trailing: bytes = b"\x00\x01\x00\x00\x00\x00") -> bytes:
    if len(zone) != ZONE_BLOCK_SIZE:
        raise ValueError("fixture zone must be exactly one block")
    stream_offset = marker_offset + STREAM_DELTA
    payload = raw_deflate(zone) + block_trailer
    data = bytearray(stream_offset)
    data[:8] = OUTER_MAGIC
    struct.pack_into(">I", data, 8, 0x22E)
    data[marker_offset:marker_offset + len(INNER_MAGIC)] = INNER_MAGIC
    data.extend(struct.pack(">H", len(payload)))
    data.extend(payload)
    data.extend(trailing)
    return bytes(data)

class Ps3FastFileInspectTests(unittest.TestCase):
    def test_accepts_four_byte_retail_block_trailer_and_end_marker(self):
        zone = b"A" * ZONE_BLOCK_SIZE
        report, expanded = inspect_bytes(make_fastfile(zone))
        self.assertTrue(report.complete)
        self.assertEqual(report.block_trailer_size, 4)
        self.assertEqual(expanded, zone)

    def test_marker_after_first_64k_is_found(self):
        zone = b"B" * ZONE_BLOCK_SIZE
        report, expanded = inspect_bytes(make_fastfile(zone, marker_offset=0x16644))
        self.assertEqual(report.inner_magic_offset, 0x16644)
        self.assertEqual(expanded, zone)

    def test_unparsed_tail_is_reported_not_silently_included(self):
        zone = b"C" * ZONE_BLOCK_SIZE
        tail = b"\x99\x88opaque-tail"
        report, expanded = inspect_bytes(make_fastfile(zone, trailing=tail))
        self.assertFalse(report.complete)
        self.assertEqual(report.trailing_bytes, len(tail))
        self.assertEqual(expanded, zone)
        self.assertIsNotNone(report.trailing_sha256)

    def test_exact_zero_terminator(self):
        report, _ = inspect_bytes(make_fastfile(b"D" * ZONE_BLOCK_SIZE, trailing=b"\x00\x00"))
        self.assertTrue(report.complete)
        self.assertEqual(report.stop_reason, "zero_terminator")

    def test_unexpected_block_trailer_is_rejected(self):
        with self.assertRaises(FastFileError):
            inspect_bytes(make_fastfile(b"E" * ZONE_BLOCK_SIZE, block_trailer=b"x"))

    def test_raw_and_inner_inflated_zones_are_both_available(self):
        body = b"asset-data"
        compressed = zlib.compress(body)
        name = b"raw/example.bin"
        header = b"\xff" * 4 + struct.pack(">II", len(compressed), len(body)) + b"\xff" * 4 + name + b"\x00"
        raw_zone = bytearray(ZONE_BLOCK_SIZE)
        offset = 0x40
        raw_zone[offset:offset + len(header)] = header
        stream = offset + len(header)
        raw_zone[stream:stream + len(compressed)] = compressed
        report, inflated, raw = inspect_bytes_full(make_fastfile(bytes(raw_zone)))
        self.assertEqual(inflated[stream:stream + len(body)], body)
        self.assertEqual(raw[stream:stream + len(compressed)], compressed)
        self.assertEqual(report.inner_streams_inflated, 1)

    def test_bad_magic_and_version_rejected(self):
        with self.assertRaises(FastFileError):
            inspect_bytes(b"not-a-fastfile")
        data = bytearray(make_fastfile(b"F" * ZONE_BLOCK_SIZE))
        struct.pack_into(">I", data, 8, 0x114)
        with self.assertRaises(FastFileError):
            inspect_bytes(bytes(data))

if __name__ == "__main__":
    unittest.main()
