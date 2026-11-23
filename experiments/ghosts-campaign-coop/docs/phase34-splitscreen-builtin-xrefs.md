# Phase 34 — Split-screen builtin cross-reference comparison

## Purpose

Phase 32 traced GSC call paths; Phases 26–29 prepared exact Campaign/Multiplayer PPU function inventories. Phase 34 connects GSC-facing split-screen builtin names to PPU functions that reference those strings.

The goal is to narrow the native local-client investigation without guessing function addresses.

## New Ghidra exporter

tools/ghidra/FindGhostsSplitScreenBuiltins.java now writes a CSV. It scans initialized memory blocks for:

- isSplitScreen
- isSplitscreenPlayer
- isSplitscreenPlayerPrimary
- isPlayingSolo
- PlayerCMD_IsSplitScreenPlayer
- PlayerCMD_IsSplitScreenPlayerPrimary
- PlayerCMD_Spawn
- PlayerCMD_ClonePlayer
- SV_InitLocalClient
- CL_InitLocalClients
- CL_SetSplitScreen

For each string hit, the exporter records Ghidra references to the string address, reference type/source, containing function name and entry, function body range, and instruction at the reference. Strings without recorded references are retained with blank reference fields.

## New comparison tool

tools/compare_splitscreen_builtin_xrefs.py joins the two xref CSVs to the matching Campaign and Multiplayer function inventories by function entry address. It normalizes Ghidra address forms such as ram:00009000 and 0x9000, then compares caller function fingerprints across builds.

It reports:
- builtins found in each executable;
- function xrefs versus data/unmapped references versus strings with no recorded xref;
- caller functions matched to the same-build PPU inventory;
- unmatched caller entries, without guessing a function;
- caller fingerprints shared across Campaign and Multiplayer;
- Multiplayer-only builtin references and Multiplayer-only caller fingerprints;
- SHA-256 values for all four input files.

## Run instructions

1. Use the exact Campaign and Multiplayer reconstructed TU 1.16 ELF pair verified by tools/ps3mapi/verify_tu116_dump.py.
2. Generate both function inventories using docs/phase28-ghidra-headless-runner.md and preserve ppu_inventory_manifest.json.
3. In Ghidra, open the Campaign program after auto-analysis. Run FindGhostsSplitScreenBuiltins.java from Script Manager and provide an output path such as D:\Ghosts\analysis\ppu-inventory\campaign_builtin_xrefs.csv.
4. Repeat on the Multiplayer program and save multiplayer_builtin_xrefs.csv.
5. Run from experiments/ghosts-campaign-coop:

~~~powershell
python tools/compare_splitscreen_builtin_xrefs.py --campaign-xrefs "D:\Ghosts\analysis\ppu-inventory\campaign_builtin_xrefs.csv" --multiplayer-xrefs "D:\Ghosts\analysis\ppu-inventory\multiplayer_builtin_xrefs.csv" --campaign-inventory "D:\Ghosts\analysis\ppu-inventory\campaign_ppu_inventory.csv" --multiplayer-inventory "D:\Ghosts\analysis\ppu-inventory\multiplayer_ppu_inventory.csv" --output "D:\Ghosts\analysis\ppu-inventory\splitscreen_builtin_xrefs.json"
~~~

## Interpretation

A string xref may point to a command-registration table, a data structure, or an instruction. The report distinguishes function callers from data/unmapped references. A matching function fingerprint is a navigation aid, not proof of identical semantics or runtime execution.

A Multiplayer-only PlayerCMD_Spawn or split-screen builtin reference is a lead for manual Ghidra inspection. It is not approval to call the command handler directly: script-command handlers may require a valid script VM context. Do not treat PlayerCMD_ClonePlayer as a real local-client creation path.

## Validation boundary

Ten synthetic Python tests cover address normalization, cross-build fingerprint matching, Multiplayer-only leads, data/unmapped references, missing function matches, duplicate rows/entries, malformed CSV, source-contract checks and input hashing.

No decrypted PPU runtime images or real Ghidra xref exports were present for this phase. Therefore this phase adds the evidence pipeline only; it does not identify a native Player 2 initializer, approve a hook, or prove Campaign co-op at runtime.
