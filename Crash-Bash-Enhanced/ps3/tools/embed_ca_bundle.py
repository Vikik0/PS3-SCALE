#!/usr/bin/env python3
"""Embed a PEM CA bundle as a C string for the PS3 TLS client."""

from pathlib import Path
import re
import sys

if len(sys.argv) != 3:
    raise SystemExit("usage: embed_ca_bundle.py INPUT.pem OUTPUT.c")

source = Path(sys.argv[1])
destination = Path(sys.argv[2])
raw = source.read_bytes()
blocks = re.findall(
    rb"-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----",
    raw,
    flags=re.DOTALL,
)
if not blocks:
    raise SystemExit("no PEM certificates found in CA bundle")

pem_lines = b"\n".join(blocks).decode("ascii").splitlines()
destination.parent.mkdir(parents=True, exist_ok=True)

with destination.open("w", encoding="ascii", newline="\n") as output:
    output.write("/* Generated from the runner's trusted CA bundle. */\n")
    output.write("#include <stddef.h>\n\n")
    output.write("const unsigned char cb_ca_bundle_pem[] =\n")
    for line in pem_lines:
        output.write('    "' + line + '\\n"\n')
    output.write("    ;\n\n")
    output.write("const size_t cb_ca_bundle_pem_len = sizeof(cb_ca_bundle_pem);\n")

print("Embedded " + str(len(blocks)) + " CA certificates into " + str(destination))
