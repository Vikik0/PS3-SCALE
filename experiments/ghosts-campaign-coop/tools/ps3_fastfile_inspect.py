#!/usr/bin/env python3
"""Read-only inspector and zone extractor for Call of Duty: Ghosts PS3 IW6 FastFiles.

Validates the outer format, locates the signed-container marker, expands the
documented raw-deflate blocks and recognized inline asset zlib streams. It never
edits or rebuilds an FF, does not verify/bypass signatures, and contains no game assets.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
import zlib
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any

OUTER_MAGIC = b"IWff0100"
INNER_MAGIC = b"IWffS100"
IW6_VERSION = 0x0000022E
STREAM_DELTA = 0x20000
ZONE_BLOCK_SIZE = 0x10000
DEFAULT_MAX_BLOCKS = 2048
MAX_ASSET_STREAM = 0x4000000
MAX_INFLATED_ASSET_BYTES = 256 * 1024 * 1024


class FastFileError(ValueError):
    """Raised when a file is not a supported PS3 Ghosts FastFile."""


@dataclass(frozen=True)
class FastFileReport:
    file_size: int
    outer_magic: str
    version: int
    inner_magic_offset: int
    stream_offset: int
    block_count: int
    outer_zone_size: int
    decompressed_size: int
    inner_streams_inflated: int
    zone_sha256: str
    complete: bool
    stop_reason: str
    trailing_bytes: int


def _inflate_raw_block(payload: bytes) -> bytes:
    decoder = zlib.decompressobj(wbits=-15)
    try:
        output = decoder.decompress(payload, ZONE_BLOCK_SIZE + 1)
    except zlib.error as exc:
        raise FastFileError(f"raw-deflate block is invalid: {exc}") from exc

    if len(output) != ZONE_BLOCK_SIZE:
        raise FastFileError(
            f"block expanded to {len(output)} bytes; expected {ZONE_BLOCK_SIZE}"
        )
    if not decoder.eof:
        raise FastFileError("raw-deflate block ended before its end marker")
    if decoder.unused_data or decoder.unconsumed_tail:
        raise FastFileError("raw-deflate block did not consume its exact payload")
    return output


def _asset_header_lengths(zone: bytes, zlib_offset: int) -> tuple[int, int] | None:
    """Return (compressed_len, decompressed_len) for a validated IW6 asset header."""
    p = zlib_offset - 1
    if p < 0 or zone[p] != 0:
        return None
    p -= 1
    name_end = p + 1
    while p > 0 and 0x20 <= zone[p] <= 0x7E and (name_end - p) < 127:
        p -= 1
    name_start = p + 1
    if name_end - name_start < 1:
        return None

    # Long scriptfile/mptype/aitype shape first, then short rawfile shape.
    for trailing_ff, size_bytes in ((8, 12), (4, 8)):
        if name_start < trailing_ff + size_bytes + 4:
            continue
        if zone[name_start - trailing_ff:name_start] != b"\xff" * trailing_ff:
            continue
        fields = name_start - trailing_ff - size_bytes
        if zone[fields - 4:fields] != b"\xff" * 4:
            continue
        comp_len = int.from_bytes(zone[fields:fields + 4], "big")
        dec_len = int.from_bytes(zone[fields + 4:fields + 8], "big")
        if 0 < comp_len < MAX_ASSET_STREAM and 0 < dec_len < MAX_ASSET_STREAM:
            return comp_len, dec_len
    return None


def _inflate_zone_assets(zone: bytes) -> tuple[bytes, int]:
    """Expand validated inline zlib asset streams while preserving their headers."""
    streams: list[tuple[int, int, bytes]] = []
    inflated_total = 0
    i = 0x40
    while i < len(zone) - 2:
        if zone[i] != 0x78 or zone[i + 1] not in (0x01, 0x5E, 0x9C, 0xDA):
            i += 1
            continue
        lengths = _asset_header_lengths(zone, i)
        if lengths is None:
            i += 1
            continue
        comp_len, dec_len = lengths
        end = i + comp_len
        if end > len(zone):
            i += 1
            continue
        decoder = zlib.decompressobj(wbits=zlib.MAX_WBITS)
        try:
            content = decoder.decompress(zone[i:end], dec_len + 1)
            if (len(content) != dec_len or not decoder.eof
                    or decoder.unused_data or decoder.unconsumed_tail):
                i += 1
                continue
        except zlib.error:
            i += 1
            continue

        inflated_total += len(content)
        if inflated_total > MAX_INFLATED_ASSET_BYTES:
            raise FastFileError("inner asset inflation exceeded the 256 MiB safety limit")
        streams.append((i, comp_len, content))
        i = end

    if not streams:
        return zone, 0
    rebuilt = bytearray()
    cursor = 0
    for start, comp_len, content in streams:
        rebuilt.extend(zone[cursor:start])
        rebuilt.extend(content)
        cursor = start + comp_len
    rebuilt.extend(zone[cursor:])
    return bytes(rebuilt), len(streams)


def inspect_bytes(data: bytes, *, max_blocks: int = DEFAULT_MAX_BLOCKS) -> tuple[FastFileReport, bytes]:
    """Inspect an FF and return its report plus the fully inflated zone."""
    if max_blocks < 1:
        raise ValueError("max_blocks must be positive")
    if len(data) < 12 or data[:8] != OUTER_MAGIC:
        raise FastFileError("missing PS3 Ghosts outer magic IWff0100")

    version = struct.unpack_from(">I", data, 8)[0]
    if version != IW6_VERSION:
        raise FastFileError(
            f"unsupported FastFile version 0x{version:08X}; expected IW6 0x{IW6_VERSION:08X}"
        )

    candidates: list[tuple[FastFileReport, bytes]] = []
    search_from = 12
    marker_limit = min(len(data) - len(INNER_MAGIC), 0x10000)
    while search_from <= marker_limit:
        marker = data.find(INNER_MAGIC, search_from, marker_limit + len(INNER_MAGIC))
        if marker < 0:
            break
        search_from = marker + 1
        stream_offset = marker + STREAM_DELTA
        if stream_offset + 2 > len(data):
            continue

        position = stream_offset
        zone_parts: list[bytes] = []
        stop_reason = "eof"
        trailing_bytes = 0

        while position < len(data):
            remaining = len(data) - position
            if remaining < 2:
                stop_reason = "truncated_block_length"
                trailing_bytes = remaining
                break

            compressed_size = struct.unpack_from(">H", data, position)[0]
            if compressed_size == 0:
                position += 2
                stop_reason = "zero_terminator"
                trailing_bytes = len(data) - position
                break

            payload_start = position + 2
            payload_end = payload_start + compressed_size
            if payload_end > len(data):
                stop_reason = "truncated_block_payload"
                trailing_bytes = remaining
                break

            try:
                block = _inflate_raw_block(data[payload_start:payload_end])
            except FastFileError:
                stop_reason = "invalid_or_non_deflate_trailing_data"
                trailing_bytes = remaining
                break

            zone_parts.append(block)
            position = payload_end
            if len(zone_parts) >= max_blocks:
                if position < len(data):
                    stop_reason = "max_blocks_reached"
                    trailing_bytes = len(data) - position
                break

        if not zone_parts:
            continue

        # Retail samples document small slack after the last valid block.
        complete = (
            stop_reason == "eof"
            or (stop_reason == "zero_terminator" and trailing_bytes == 0)
            or (stop_reason in ("invalid_or_non_deflate_trailing_data", "truncated_block_payload") and 0 < trailing_bytes <= 64)
        )
        raw_zone = b"".join(zone_parts)
        zone, inner_streams = _inflate_zone_assets(raw_zone)
        report = FastFileReport(
            file_size=len(data),
            outer_magic=OUTER_MAGIC.decode("ascii"),
            version=version,
            inner_magic_offset=marker,
            stream_offset=stream_offset,
            block_count=len(zone_parts),
            outer_zone_size=len(raw_zone),
            decompressed_size=len(zone),
            inner_streams_inflated=inner_streams,
            zone_sha256=hashlib.sha256(zone).hexdigest(),
            complete=complete,
            stop_reason=stop_reason,
            trailing_bytes=trailing_bytes,
        )
        candidates.append((report, zone))

    if not candidates:
        raise FastFileError(
            "IW6 signed-container marker found no valid 64 KiB raw-deflate block stream"
        )

    candidates.sort(
        key=lambda item: (item[0].block_count, item[0].complete, -item[0].trailing_bytes),
        reverse=True,
    )
    return candidates[0]


def inspect_file(path: Path, *, max_blocks: int = DEFAULT_MAX_BLOCKS) -> tuple[FastFileReport, bytes]:
    try:
        data = path.read_bytes()
    except OSError as exc:
        raise FastFileError(f"cannot read input file: {exc}") from exc
    return inspect_bytes(data, max_blocks=max_blocks)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fastfile", type=Path, help="user-owned Ghosts PS3 .ff file")
    parser.add_argument("--output-zone", type=Path, help="write the fully inflated zone when the block stream is complete")
    parser.add_argument("--report-json", type=Path, help="also write the inspection report as JSON")
    parser.add_argument("--max-blocks", type=int, default=DEFAULT_MAX_BLOCKS)
    args = parser.parse_args(argv)

    try:
        report, zone = inspect_file(args.fastfile, max_blocks=args.max_blocks)
        if args.output_zone:
            if not report.complete:
                raise FastFileError(
                    f"refusing to write an incomplete zone ({report.stop_reason}; "
                    f"{report.trailing_bytes} trailing bytes)"
                )
            if args.output_zone.resolve() == args.fastfile.resolve():
                raise FastFileError("output path must not overwrite the input FastFile")
            args.output_zone.parent.mkdir(parents=True, exist_ok=True)
            args.output_zone.write_bytes(zone)

        payload: dict[str, Any] = asdict(report)
        if args.output_zone:
            payload["output_zone"] = str(args.output_zone)
        rendered = json.dumps(payload, indent=2, sort_keys=True)
        print(rendered)
        if args.report_json:
            args.report_json.parent.mkdir(parents=True, exist_ok=True)
            args.report_json.write_text(rendered + "\n", encoding="utf-8")
        return 0 if report.complete else 2
    except (FastFileError, OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
