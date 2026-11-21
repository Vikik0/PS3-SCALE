import struct
import unittest
import zlib

from tools.ps3_fastfile_inspect import (
    FastFileError,
    INNER_MAGIC,
    OUTER_MAGIC,
    STREAM_DELTA,
    ZONE_BLOCK_SIZE,
    inspect_bytes,
)


def raw_deflate(data: bytes) -> bytes:
    compressor = zlib.compressobj(level=9, wbits=-15)
    return compressor.compress(data) + compressor.flush()


def make_fastfile(zone: bytes, *, marker_offset: int = 0x24, trailing: bytes = b"") -> bytes:
    if len(zone) != ZONE_BLOCK_SIZE:
        raise ValueError("fixture zone must be exactly one block")
    stream_offset = marker_offset + STREAM_DELTA
    payload = raw_deflate(zone)
    if len(payload) > 0xFFFF:
        raise ValueError("fixture compressed block too large")
    data = bytearray(stream_offset)
    data[:8] = OUTER_MAGIC
    struct.pack_into(">I", data, 8, 0x22E)
    if marker_offset == 0x24:
        struct.pack_into(">I", data, 12, 0x01000404)
    data[marker_offset:marker_offset + len(INNER_MAGIC)] = INNER_MAGIC
    data.extend(struct.pack(">H", len(payload)))
    data.extend(payload)
    data.extend(trailing)
    return bytes(data)


class Ps3FastFileInspectTests(unittest.TestCase):
    def test_patch_fastfile_variant(self):
        zone = bytes((index * 17) & 0xFF for index in range(ZONE_BLOCK_SIZE))
        report, expanded = inspect_bytes(make_fastfile(zone))
        self.assertEqual(report.inner_magic_offset, 0x24)
        self.assertEqual(report.stream_offset, 0x20024)
        self.assertEqual(report.block_count, 1)
        self.assertTrue(report.complete)
        self.assertEqual(expanded, zone)

    def test_base_fastfile_variant_uses_discovered_marker_offset(self):
        pattern = b"GHOSTS-IW6-PS3"
        zone = (pattern * ((ZONE_BLOCK_SIZE + len(pattern) - 1) // len(pattern)))[:ZONE_BLOCK_SIZE]
        report, expanded = inspect_bytes(make_fastfile(zone, marker_offset=0x3294))
        self.assertEqual(report.inner_magic_offset, 0x3294)
        self.assertEqual(report.stream_offset, 0x23294)
        self.assertEqual(expanded, zone)

    def test_trailing_bytes_are_reported_and_not_included(self):
        zone = b"A" * ZONE_BLOCK_SIZE
        report, expanded = inspect_bytes(make_fastfile(zone, trailing=b"TAIL"))
        self.assertFalse(report.complete)
        self.assertEqual(report.trailing_bytes, 4)
        self.assertEqual(expanded, zone)

    def test_wrong_outer_magic_is_rejected(self):
        with self.assertRaises(FastFileError):
            inspect_bytes(b"NOT_A_FF!" + struct.pack(">I", 0x22E) + bytes(100))

    def test_wrong_version_is_rejected(self):
        data = bytearray(make_fastfile(b"B" * ZONE_BLOCK_SIZE))
        struct.pack_into(">I", data, 8, 0x114)
        with self.assertRaises(FastFileError):
            inspect_bytes(bytes(data))

    def test_missing_inner_marker_is_rejected(self):
        data = bytearray(make_fastfile(b"C" * ZONE_BLOCK_SIZE))
        marker = data.find(INNER_MAGIC)
        data[marker:marker + len(INNER_MAGIC)] = b"XXXXXXXX"
        with self.assertRaises(FastFileError):
            inspect_bytes(bytes(data))

    def test_truncated_payload_is_rejected(self):
        data = make_fastfile(b"D" * ZONE_BLOCK_SIZE)
        with self.assertRaises(FastFileError):
            inspect_bytes(data[:-3])

    def test_inner_zlib_asset_is_inflated_inline(self):
        body = b"main() { return; }"
        compressed = zlib.compress(body)
        name = b"maps/mp/example.gsc"
        header = (
            b"\xff" * 4
            + struct.pack(">II", len(compressed), len(body))
            + b"\xff" * 4
            + name
            + b"\x00"
        )
        raw_zone = bytearray(ZONE_BLOCK_SIZE)
        asset_offset = 0x40
        raw_zone[asset_offset:asset_offset + len(header)] = header
        stream_offset = asset_offset + len(header)
        raw_zone[stream_offset:stream_offset + len(compressed)] = compressed

        report, inflated_zone = inspect_bytes(make_fastfile(bytes(raw_zone)))
        self.assertEqual(report.inner_streams_inflated, 1)
        self.assertIn(body, inflated_zone)
        self.assertEqual(
            report.decompressed_size,
            ZONE_BLOCK_SIZE - len(compressed) + len(body),
        )

    def test_max_block_limit_is_enforced(self):
        zone = b"E" * ZONE_BLOCK_SIZE
        one = make_fastfile(zone)
        report, _ = inspect_bytes(one, max_blocks=1)
        self.assertEqual(report.block_count, 1)


if __name__ == "__main__":
    unittest.main()
