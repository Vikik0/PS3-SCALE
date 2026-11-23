# Phase 35 — Atomic Ghidra builtin-XREF pipeline

## Purpose

Phase 34 added a split-screen builtin xref exporter and comparer. Phase 35 removes the remaining manual Ghidra step by running the xref exporter automatically in the existing headless Campaign/Multiplayer inventory workflow.

## Runner changes

tools/run_ghidra_ppu_inventory.py now executes two post-scripts per build, in order:

1. ExportGhostsPpuFunctionInventory.java
2. FindGhostsSplitScreenBuiltins.java

The runner validates both CSV outputs against their schemas before promoting any final files. Campaign and Multiplayer remain in separate temporary Ghidra projects. If either build, either exporter, or either validation step fails, no partial output set is promoted.

## Output set

The output directory now contains five files:

- campaign_ppu_inventory.csv
- multiplayer_ppu_inventory.csv
- campaign_builtin_xrefs.csv
- multiplayer_builtin_xrefs.csv
- ppu_inventory_manifest.json

The manifest keeps the original ELF and function-inventory provenance and adds the XREF filename, SHA-256 and row count for each build. The five-file output set is promoted atomically, with rollback behavior retained if promotion fails.

## Run

Use the existing Phase 28 command unchanged:

~~~powershell
python tools/run_ghidra_ppu_inventory.py --analyze-headless "C:\Tools\ghidra\support\analyzeHeadless.bat" --campaign-elf "D:\Ghosts\analysis\campaign_tu116_runtime.elf" --multiplayer-elf "D:\Ghosts\analysis\multiplayer_tu116_runtime.elf" --output-dir "D:\Ghosts\analysis\ppu-inventory"
~~~

Then run the Phase 34 comparer using the two XREF CSVs and two function inventories produced in that directory.

## Tests and limits

The runner tests now verify that both post-scripts are included for both builds, xref rows are matched against the same-build inventory, XREF hashes/counts enter the manifest, and malformed XREF output prevents partial promotion.

GitHub Actions run 45255419594 passed all five workflow jobs, including 306 Python tests and the PS3 PRX bootstrap. The runner uses a mocked Ghidra process in host tests; CI does not execute Ghidra itself. No real decrypted PPU pair was available, so no real split-screen caller or native Player 2 initializer has been identified.
