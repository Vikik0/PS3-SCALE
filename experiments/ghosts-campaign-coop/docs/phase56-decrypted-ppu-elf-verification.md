# Phase 56 — Repair the decrypted PPU ELF verification gate

## Bug found

The TU 1.16 input verifier read the program-header count from a field name that the PpuElf parser does not expose. The parser exposes program_header_count. Encrypted SELF-only checks did not exercise this branch, so supplying decrypted Campaign/Multiplayer ELFs could raise a KeyError instead of completing preflight.

## Changes

- Read the program-header count from PpuElf.header["program_header_count"].
- Keep the eight-header TU 1.16 check and include explicit PowerPC64/big-endian metadata in each decrypted-ELF report.
- Add synthetic ELF64 BE PowerPC64 fixtures with eight PT_LOAD headers.
- Add a positive test that verifies both expected entry points and reaches native_ppu_comparison_ready=true.
- Add a negative test that rejects a Campaign ELF with the wrong entry point.

## Validation boundary

The tests exercise the real PpuElf parser and verifier against synthetic binaries. They do not decrypt or inspect retail PPU instructions. Real Ghidra analysis still requires the matching locally decrypted TU 1.16 Campaign and Multiplayer ELF pair.
