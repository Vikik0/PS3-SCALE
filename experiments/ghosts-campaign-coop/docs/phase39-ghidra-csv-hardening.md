# Phase 39 — Ghidra CSV output hardening

## Issues closed

- The XREF exporter previously left is_call and is_data blank when a string had no recorded references. The Python reader correctly rejects blank boolean values, so an otherwise valid scan containing an unreferenced string could fail the entire pipeline.
- The function-inventory exporter used the host default character encoding while the Python reader expects UTF-8. This could corrupt non-ASCII symbol/function names on some Windows installations.

## Changes

- No-reference XREF rows now explicitly write is_call=false and is_data=false.
- Both ExportGhostsPpuFunctionInventory.java and FindGhostsSplitScreenBuiltins.java use Files.newBufferedWriter with StandardCharsets.UTF_8.
- Static source-contract tests pin the no-reference behavior and UTF-8 writer configuration.
- Phase 34 documentation now describes the exact CSV contract consumed by the strict Python parser.

## Validation

The Phase 39 CI run passed all five jobs and 312 Python tests, including the no-reference boolean defaults and UTF-8 exporter checks.

No Ghidra binary is bundled or executed in GitHub Actions. The real exporter still requires the user's installed Ghidra and exact decrypted TU 1.16 PPU pair.
