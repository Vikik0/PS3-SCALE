# Phase 41 — Bound OPD seeding to the PPU address space

## Problem

The OPD helper accepts an address and size from the ELF inspector. Before this phase, it checked alignment and mapped endpoints but did not explicitly reject an address above the 32-bit PPU pointer range or an address+size pair that crossed the end of that range.

## Changes

- The Python runner validates both CLI strings and direct API tuples against the unsigned 32-bit PPU address space.
- The range must be fully contained in 0x00000000–0xFFFFFFFF; address+size may not exceed 0x100000000. Phase 42 additionally requires an 8-byte-aligned OPD table base and rejects unaligned 4-byte PPU code pointers.
- The Java Ghidra seeder repeats the same checks before calling toAddr() or reading descriptor memory. Phase 43 also requires the seeder to emit a machine-readable result and throw on invalid or empty seeding.
- Existing alignment and mapped-range checks remain in place.
- Tests cover an out-of-range base address, an overflowing range, and the Java source guard.

## Validation

GitHub Actions run 45263357751 passed all five jobs and 315 Python tests, including the Phase 41 OPD boundary tests.

This is input validation only. OPD values still must come from the exact user's ELF inspector output; no range is guessed, and no hook is approved.
