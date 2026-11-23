# Phase 28 — Ghidra headless inventory runner

## Purpose

The Phase 26/27 workflow required the operator to open two reconstructed PPU ELFs in Ghidra, run the correct scripts in the correct order, export two CSV files, and then compare them. That manual sequence creates opportunities to select the wrong processor, mix Campaign and Multiplayer outputs, or overwrite an earlier capture.

Phase 28 adds one host-side runner that performs the two Ghidra headless imports consistently and promotes the outputs only after both inventories validate.

## What it does

- Runs Campaign and Multiplayer in separate temporary Ghidra projects.
- Selects the explicit processor language PowerPC:BE:64:default.
- Runs the OPD function seeding script only when the operator supplies both an OPD virtual address and size for that build.
- Runs the PPU inventory exporter after optional OPD seeding.
- Validates each CSV with the same parser used by the comparison tool, including unique function entry addresses and SHA-256 fingerprints.
- Writes a manifest with each reconstructed ELF's SHA-256, each CSV's SHA-256, function counts, declared profile, OPD settings and the executed command arguments.
- Stages both inventories and the manifest before promotion. If either Ghidra run or CSV validation fails, no partial final output is promoted.
- Refuses to overwrite existing outputs unless --overwrite is explicit.

## Windows usage

From the experiments/ghosts-campaign-coop directory:

~~~powershell
python tools/run_ghidra_ppu_inventory.py --analyze-headless "C:\Tools\ghidra\support\analyzeHeadless.bat" --campaign-elf "D:\Ghosts\analysis\campaign_tu116_runtime.elf" --multiplayer-elf "D:\Ghosts\analysis\multiplayer_tu116_runtime.elf" --output-dir "D:\Ghosts\analysis\ppu-inventory"
~~~

If and only if the ELF inspector reports the OPD range for a specific build, add that build's pair of arguments:

~~~powershell
--campaign-opd-va "0x..." --campaign-opd-size "0x..."
--multiplayer-opd-va "0x..." --multiplayer-opd-size "0x..."
~~~

Do not guess OPD values. Campaign and Multiplayer can have different OPD ranges.

Then compare the generated inventories:

~~~powershell
python tools/compare_ppu_function_inventories.py --campaign "D:\Ghosts\analysis\ppu-inventory\campaign_ppu_inventory.csv" --multiplayer "D:\Ghosts\analysis\ppu-inventory\multiplayer_ppu_inventory.csv" --manifest "D:\Ghosts\analysis\ppu-inventory\ppu_inventory_manifest.json" --output "D:\Ghosts\analysis\ppu-inventory\ppu_function_comparison.json"
~~~

Review ppu_inventory_manifest.json first and retain it with the comparison JSON.
When the manifest is supplied, the comparer verifies both CSV SHA-256 values, filenames, function counts, source ELF SHA-256 fields and the operator-declared BLUS31270/TU 1.16/PowerPC profile. It rejects a mismatch before writing the comparison report. This cross-check confirms the files against the manifest; it does not independently authenticate the game update.
 Its profile label is operator-declared; the recorded SHA-256 values identify the exact input files but do not independently prove the game update's authenticity.

## Validation boundary

CI tests the runner with a mocked Ghidra process. It verifies command construction, optional OPD seeding, input/output hashing, malformed inventory rejection, overwrite protection, and no partial promotion when the second analysis fails. CI does not include Ghidra itself and does not run the Java scripts.

The runner is a workflow improvement, not runtime evidence. It does not discover the correct TU automatically, create a local client, identify a hook, modify game files, or prove Player 2 in Campaign.
