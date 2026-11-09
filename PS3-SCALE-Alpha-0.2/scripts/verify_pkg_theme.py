#!/usr/bin/env python3
"""Verify the bundled theme is physically present in a finalized PS3 PKG."""
from __future__ import annotations

import hashlib
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

PKG_AES_KEY = "2E7B71D7C9C9A14EA3221F188828B8F8"
EXPECTED_SIZE = 5_182_732
EXPECTED_SHA256 = "b8a7b1c2573ea8a397c2fbc3a6c731b25dcb71f572aedf5af8568785620d1b64"
TARGET_NAME = "USRDIR/theme.mp3"
LOGO_NAME = "USRDIR/apollo-save.png"
LOGO_SIZE = 5_046
LOGO_SHA256 = "a9efad28fbd8788fb0129c71de78c70e45d9fd47ab0de461124eb9ec2a930465"


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: verify_pkg_theme.py <finalized.pkg>")

    package_path = Path(sys.argv[1])
    package = package_path.read_bytes()
    if len(package) < 0xC0 or package[:4] != bytes.fromhex("7f504b47"):
        raise SystemExit("ERROR: invalid PS3 PKG header")

    revision = struct.unpack_from(">H", package, 0x04)[0]
    if revision != 0x8000:
        raise SystemExit(f"ERROR: expected finalized PKG revision 0x8000, got {revision:#06x}")

    item_count = struct.unpack_from(">I", package, 0x14)[0]
    data_offset = struct.unpack_from(">Q", package, 0x20)[0]
    data_size = struct.unpack_from(">Q", package, 0x28)[0]
    iv = package[0x70:0x80]

    if data_offset + data_size > len(package):
        raise SystemExit("ERROR: PKG encrypted data range exceeds file size")

    with tempfile.TemporaryDirectory(prefix="ps3scale-pkg-check-") as temp_dir:
        encrypted_path = Path(temp_dir) / "encrypted.bin"
        decrypted_path = Path(temp_dir) / "decrypted.bin"
        encrypted_path.write_bytes(package[data_offset:data_offset + data_size])
        subprocess.run(
            [
                "openssl", "enc", "-d", "-aes-128-ctr",
                "-K", PKG_AES_KEY,
                "-iv", iv.hex(),
                "-in", str(encrypted_path),
                "-out", str(decrypted_path),
            ],
            check=True,
            stdout=subprocess.DEVNULL,
        )
        payload = decrypted_path.read_bytes()

    found = False
    logo_found = False
    for index in range(item_count):
        record_offset = index * 0x20
        if record_offset + 0x20 > len(payload):
            raise SystemExit("ERROR: truncated PKG item table")
        name_offset, name_size, file_offset, file_size, flags, _ = struct.unpack_from(
            ">IIQQII", payload, record_offset
        )
        if name_offset + name_size > len(payload) or file_offset + file_size > len(payload):
            raise SystemExit("ERROR: PKG item points outside decrypted data")
        name = payload[name_offset:name_offset + name_size].decode("utf-8", "strict")
        if name == TARGET_NAME:
            audio = payload[file_offset:file_offset + file_size]
            digest = hashlib.sha256(audio).hexdigest()
            if len(audio) != EXPECTED_SIZE:
                raise SystemExit(f"ERROR: embedded theme size mismatch: {len(audio)}")
            if digest != EXPECTED_SHA256:
                raise SystemExit(f"ERROR: embedded theme SHA-256 mismatch: {digest}")
            print(f"PASS: {TARGET_NAME} is embedded in {package_path.name}")
            print(f"PASS: {len(audio):,} bytes; SHA-256 {digest}")
            found = True
        elif name == LOGO_NAME:
            logo = payload[file_offset:file_offset + file_size]
            logo_digest = hashlib.sha256(logo).hexdigest()
            if len(logo) != LOGO_SIZE:
                raise SystemExit(f"ERROR: embedded Apollo logo size mismatch: {len(logo)}")
            if logo_digest != LOGO_SHA256:
                raise SystemExit(f"ERROR: embedded Apollo logo SHA-256 mismatch: {logo_digest}")
            print(f"PASS: {LOGO_NAME} is embedded in {package_path.name}")
            print(f"PASS: {len(logo):,} bytes; SHA-256 {logo_digest}")
            logo_found = True

    if not found:
        raise SystemExit(f"ERROR: {TARGET_NAME} is missing from the PKG")
    if not logo_found:
        raise SystemExit(f"ERROR: {LOGO_NAME} is missing from the PKG")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
