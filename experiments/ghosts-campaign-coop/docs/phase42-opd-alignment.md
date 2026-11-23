# Phase 42 — Validate OPD and PPU instruction alignment

## Changes

- The OPD table virtual address must be 8-byte aligned, matching the 8-byte descriptor iteration used by the PS3 PPU parser.
- Every descriptor's code pointer must be 4-byte aligned before Ghidra attempts disassembly/function creation.
- The Python runner applies the OPD-base alignment rule for both CLI strings and direct API tuples.
- The Java seeder repeats the OPD-base check and rejects unaligned code pointers.
- Existing size-alignment, 32-bit address-space, range-overflow and mapped-memory checks remain active.

## Tests

One additional host test covers misaligned OPD base values for both the CLI parser and direct API validator. The Java source-contract test checks both the OPD and code-pointer alignment guards.

## Validation boundary

Alignment checks reject malformed descriptor inputs; they do not prove that every aligned descriptor is a valid function. The ELF inspector's exact OPD range and Ghidra's mapped executable blocks remain required.
