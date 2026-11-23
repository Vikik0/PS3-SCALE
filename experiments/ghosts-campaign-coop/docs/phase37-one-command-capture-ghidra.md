# Phase 37 — One-command TU 1.16 capture and Ghidra analysis

## Purpose

The project already had a read-only PS3MAPI capture analyzer and a separate Ghidra headless PPU pipeline. Phase 37 connects them with an optional flag so the exact Campaign/Multiplayer capture pair can move from verification to PPU function/XREF comparison in one local command.

## Behavior

tools/ps3mapi/analyze_tu116_runtime_capture.py accepts an optional analyze_headless path. After it verifies both SELF/dump pairs and reconstructs the two analysis ELFs, it calls tools/run_ghidra_ppu_inventory.py against those exact staged ELFs.

Optional arguments:
- ghidra_script_dir selects the Ghidra Java scripts; by default the runner uses the repository tools/ghidra folder.
- campaign_opd and multiplayer_opd are accepted only as complete address/size pairs. Supply them only when the ELF inspector reported the exact values for each build.
- Ghidra script directory and OPD settings are rejected unless analyze_headless is provided.

## Outputs

Without Ghidra, the analyzer retains its original ten outputs.

With Ghidra enabled, it adds six outputs:
- ppu_campaign_ppu_inventory.csv
- ppu_multiplayer_ppu_inventory.csv
- ppu_campaign_builtin_xrefs.csv
- ppu_multiplayer_builtin_xrefs.csv
- ppu_splitscreen_builtin_xrefs.json
- ppu_inventory_manifest.json

capture_summary.json records whether PPU analysis ran, function counts, XREF counts, Multiplayer-only lead count, and the hashes of the comparison and manifest. The PPU manifest's ELF paths are rewritten to the final capture-output paths, while retaining the exact ELF SHA-256 values and command provenance.

## Atomicity and failure handling

The analyzer determines the final output set before starting. It stages the capture and PPU evidence together. It promotes either the original ten outputs or all sixteen outputs only after every requested step succeeds. A Ghidra failure, missing output, malformed XREF, comparison failure or promotion error prevents a partial output set from being left behind. Existing outputs are preserved unless overwrite is explicitly requested.

## Usage

Use the Phase 10 command and append:

~~~powershell
--analyze-headless "C:\Tools\ghidra\support\analyzeHeadless.bat" --ghidra-script-dir "D:\Ghosts\PS3-SCALE\experiments\ghosts-campaign-coop\tools\ghidra"
~~~

Add OPD arguments only when verified:

~~~powershell
--campaign-opd-va "0x..." --campaign-opd-size "0x..." --multiplayer-opd-va "0x..." --multiplayer-opd-size "0x..."
~~~

## CI result

GitHub Actions run 45256740635 passed all five jobs and 309 Python tests. The Ghidra integration is tested with a mocked runner; no actual Ghidra process or retail PPU ELF is executed by CI.

## Validation boundary

Host tests mock the Ghidra runner and verify output copying, summary metadata, option validation and failure rollback. CI does not run Ghidra itself. The tool still needs the user's exact TU 1.16 SELF files, PS3MAPI dumps and Ghidra installation to produce real PPU findings. It does not approve hooks or prove a playable Campaign Player 2.
