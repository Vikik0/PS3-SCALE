# Phase 44 — Reconcile every OPD descriptor slot

## Problem

The schema-1 OPD report called non-empty descriptors "descriptors_visited" and combined unreadable descriptors with invalid code pointers in "invalid_entries". That made it impossible for the host runner to prove that every 8-byte slot in the requested OPD range had been accounted for. A malformed or incomplete summary could still satisfy the earlier, weaker count checks.

## Changes

- OPD summary schema advances to version 2.
- The seeder reports descriptor_slots_scanned, empty_descriptors, nonempty_descriptors, and unreadable_descriptors separately.
- Non-empty descriptors are split into valid_code_pointers and invalid_code_pointers; a zero code pointer is explicitly invalid even if address zero happens to be mapped executable.
- The runner requires descriptor_slots_scanned == opd_size / 8.
- It requires empty + non-empty + unreadable descriptors to equal all scanned slots.
- It requires valid + invalid code pointers to equal all non-empty descriptors.
- It retains function-creation reconciliation and TOC mapping reconciliation, and additionally ensures distinct non-zero TOC values cannot exceed the number of descriptors with non-zero TOC.
- Direct Python API calls reject booleans as OPD integers instead of accepting False as address zero.

## Validation boundary

Host tests exercise the schema and accounting rules with a mocked Ghidra process. CI does not execute the actual Java seeder or validate a retail OPD table. The exact TU 1.16 decrypted PPU ELF and operator-supplied OPD range remain necessary for real evidence.
