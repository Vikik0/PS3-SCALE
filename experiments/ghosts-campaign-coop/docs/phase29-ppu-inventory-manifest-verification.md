# Phase 29 — PPU inventory manifest verification

## Purpose

Phase 28 generates a manifest that records the exact reconstructed ELF hashes, the exported CSV hashes, function counts, and the operator-declared TU 1.16 profile. Until Phase 29, the comparer did not consume that manifest, so a user could accidentally compare a CSV that had been edited, renamed, or copied from a different analysis run.

Phase 29 connects the manifest to the comparison step.

## Changes

- Added an optional --manifest argument to compare_ppu_function_inventories.py.
- When supplied, the comparer verifies the manifest schema and the declared BLUS31270 / TU 1.16 / PowerPC:BE:64:default profile.
- It verifies each inventory filename, SHA-256 digest, and function-record count against the corresponding CSV.
- It checks that the manifest records a valid SHA-256 for each source ELF.
- The comparison JSON records the manifest path/hash, the two source ELF hashes, and the validated function counts.
- Without a manifest, legacy CSV comparison remains available but the report explicitly marks provenance as NOT_PROVIDED.
- A manifest mismatch fails closed before the comparison JSON is written.

## Usage

~~~powershell
python tools/compare_ppu_function_inventories.py --campaign "D:\Ghosts\analysis\ppu-inventory\campaign_ppu_inventory.csv" --multiplayer "D:\Ghosts\analysis\ppu-inventory\multiplayer_ppu_inventory.csv" --manifest "D:\Ghosts\analysis\ppu-inventory\ppu_inventory_manifest.json" --output "D:\Ghosts\analysis\ppu-inventory\ppu_function_comparison.json"
~~~

Use the manifest produced by the same run_ghidra_ppu_inventory.py invocation as the two CSV files. Do not mix a manifest from another capture run.

## Validation boundary

The manifest is a local provenance record. Its target profile is operator-declared, and its hashes bind the CSVs to the recorded ELF files; neither property independently proves that the ELF files are authentic retail BLUS31270 TU 1.16 images. The original SELF and dump verification reports remain the source of capture identity.

No real PPU inventories were supplied for this phase. Synthetic tests verify the manifest cross-check only; they do not identify a Player 2 function or hook.
