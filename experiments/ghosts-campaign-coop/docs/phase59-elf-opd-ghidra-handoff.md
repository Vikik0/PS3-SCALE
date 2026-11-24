# Phase 59 — Exercise the complete ELF-to-OPD handoff

## Purpose

Phase 56 tested the decrypted-ELF verifier with real PpuElf parsing, while Phase 54's orchestrator tests mocked inspect_file. That left the combined path from a structurally valid PPU ELF through OPD range discovery and into the Ghidra runner without one end-to-end host test.

## Changes

- Add minimal ELF64 big-endian PowerPC64 fixtures with executable code segments and separate OPD data sections.
- Run the real ps3_elf_inspect.inspect_file for both Campaign and Multiplayer inputs.
- Verify that each build independently produces its own OPD seed-range candidate.
- Verify that the temporary inspector reports passed to the Ghidra runner contain the exact ELF SHA-256 and the matching OPD virtual address/size.
- Keep Ghidra itself mocked; no native process is executed by CI.

## Validation boundary

This is an end-to-end synthetic ELF/OPD handoff test. It validates parser/orchestrator wiring, not the OPD layout of the user's retail ELF or the correctness of any function hook.
