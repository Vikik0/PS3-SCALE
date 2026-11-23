# Phase 26 — PPU call-target symbol inventory

## Purpose

The existing Campaign/Multiplayer PPU inventory comparison ranked Multiplayer-only functions mainly by their own Ghidra function names. That can miss useful code when auto-analysis leaves a candidate named `FUN_...` but resolves one or more of its call targets to a meaningful function symbol.

Phase 26 preserves those resolved call-target names in the inventory and uses them as a second, separate ranking signal.

## Changes

- The Ghidra exporter now writes an optional `call_target_symbols` CSV column beside the existing call-target address list.
- Each resolved entry is stored as `address:function_name`; unresolved targets are explicitly labelled `UNRESOLVED`.
- The Python inventory reader accepts the new column but continues to load legacy CSV files without it.
- Multiplayer-only functions can now be ranked by split-screen, local-client, controller, player, camera, viewport, HUD, connect, spawn and input terms found in their resolved call-target symbols.
- The report keeps function-name matches and call-target-symbol matches as separate counts, so the source of each lead is visible.

## Use

Run `ExportGhostsPpuFunctionInventory.java` separately in Ghidra on the exact verified TU 1.16 Campaign and Multiplayer decrypted PPU images, then compare the two CSV files with `tools/compare_ppu_function_inventories.py`.

The Ghidra inventory must be generated after auto-analysis and, where appropriate, after using the OPD seeding helper with OPD bounds obtained from the ELF inspector. Do not guess the OPD range.

## Validation boundary

This change improves static-analysis navigation only. A call-target symbol match is not proof that the function creates a local client, and it does not approve a hook address or prove that a Multiplayer routine is safe in Campaign. Exact executable hashes, calling convention, TOC, global state, runtime lifecycle and target-console evidence remain required.

No decrypted PPU runtime image or function inventory was supplied for this phase. Therefore no new native Player 2 function or hook has been identified.
