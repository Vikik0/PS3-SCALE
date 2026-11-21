#!/usr/bin/env python3
"""Validate the generated PS3 PKG headers before publishing a CI artifact."""

from __future__ import annotations

import hashlib
import struct
import sys
from pathlib import Path

CONTENT_ID = b"UP0001-CBAS00001_00-CRASHBASHONLINE1"
PKG_MAGIC = b"\x7fPKG"
HEADER_MIN_SIZE = 0x80


def verify(path: Path) -> tuple[str, int]:
    data = path.read_bytes()
    if len(data) < HEADER_MIN_SIZE:
        raise ValueError(f"{path}: file is shorter than a PS3 PKG header")
    if data[:4] != PKG_MAGIC:
        raise ValueError(f"{path}: invalid PKG magic {data[:4].hex()}")

    declared_size = struct.unpack_from(">I", data, 0x1C)[0]
    if declared_size != len(data):
        raise ValueError(
            f"{path}: header declares {declared_size} bytes, file has {len(data)}"
        )

    data_offset = struct.unpack_from(">Q", data, 0x20)[0]
    data_size = struct.unpack_from(">Q", data, 0x28)[0]
    if data_offset < HEADER_MIN_SIZE or data_offset > len(data):
        raise ValueError(f"{path}: invalid data offset {data_offset}")
    if data_size == 0 or data_size > len(data) - data_offset:
        raise ValueError(f"{path}: invalid payload size {data_size}")

    content_id = data[0x30 : 0x30 + len(CONTENT_ID)]
    if content_id != CONTENT_ID:
        raise ValueError(
            f"{path}: unexpected content ID {content_id.decode('ascii', 'replace')!r}"
        )

    digest = hashlib.sha256(data).hexdigest()
    print(f"OK {path}: {len(data)} bytes, {content_id.decode()}, sha256={digest}")
    return digest, len(data)


def main() -> int:
    if len(sys.argv) != 3:
        print(
            "usage: verify_pkg.py <installer.pkg> <installer.gnpdrm.pkg>",
            file=sys.stderr,
        )
        return 2

    try:
        plain_hash, plain_size = verify(Path(sys.argv[1]))
        final_hash, final_size = verify(Path(sys.argv[2]))
        if plain_size != final_size:
            raise ValueError("debug and GNPDRM package sizes differ")
        if plain_hash == final_hash:
            raise ValueError("GNPDRM package is byte-identical to the debug PKG")
    except (OSError, ValueError, struct.error) as exc:
        print(f"PKG verification failed: {exc}", file=sys.stderr)
        return 1

    print("PS3 PKG verification passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
