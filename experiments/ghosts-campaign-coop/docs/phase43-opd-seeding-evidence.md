# Phase 43 — Fail-closed OPD seeding with structured evidence

## Problem

Ghidra scripts can print an error and return normally. The headless process may then continue to export a function inventory, making the manifest say OPD seeding was enabled even though the seeder never validated or created any functions.

## Changes

- SeedGhostsOpdFunctions.java now requires a third argument: a summary JSON path.
- Invalid arguments, misaligned/out-of-range/unmapped OPD tables and empty/no-function seed results throw exceptions instead of returning success.
- The seeder writes a UTF-8 JSON report containing the exact OPD range, descriptors visited, executable code pointers, functions created, functions already present, failures to create, and invalid entries.
- The Python runner requires the report whenever OPD seeding is requested.
- The runner validates schema/status, exact address and size, non-negative counts, count reconciliation, and at least one created/already-present function.
- The PPU manifest embeds the validated report and its SHA-256. If the report is missing or invalid, none of the six final evidence files are promoted.

## Validation

Synthetic tests cover successful summary ingestion, missing summary rejection, mismatched-range rejection, zero seeded functions, and manifest hash recording. CI does not execute Ghidra; it tests the runner with a mocked headless process and summary output.
