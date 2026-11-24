# Phase 54 — One-command local TU 1.16 native analysis

## Purpose

The earlier tools required separate manual steps: verify retail SELF inputs, inspect decrypted Campaign and Multiplayer ELFs, copy OPD ranges into the Ghidra runner, and then compare the two builds. Phase 54 joins those steps while preserving the existing validation gates.

## Changes

- Added tools/run_ghosts_tu116_native_analysis.py.
- It runs the Phase 53 local retail-input verifier first. A mismatch blocks all ELF inspection and Ghidra work.
- If both decrypted ELFs are present, it validates them against the installed TU 1.16 entry points and PPU ELF format.
- It derives OPD seed-range candidates independently from each ELF using ps3_elf_inspect, writes temporary hash-bound inspector reports, and passes those reports to run_ghidra_ppu_inventory.
- It never asks the operator to type OPD addresses or sizes. If either ELF lacks a valid OPD range, it stops instead of guessing.
- Ghidra inventory generation remains atomic. The orchestrator writes native_input_preflight.json and native_analysis_status.json beside the existing inventory bundle.
- If encrypted SELF files are verified but decrypted ELFs are absent, it reports WAITING_FOR_BOTH_DECRYPTED_PPU_ELFS and exits without launching Ghidra.

## Local invocation

    python tools/run_ghosts_tu116_native_analysis.py --disc-root "D:\Games\Ghosts\PS3_GAME" --update-root "D:\RPCS3\dev_hdd0\game\BLUS31270" --campaign-elf "D:\Ghosts\decrypted\default.elf" --multiplayer-elf "D:\Ghosts\decrypted\default_mp.elf" --analyze-headless "C:\ghidra\support\analyzeHeadless.bat"

Add --overwrite only when replacing an existing output bundle. Before writing any preflight report, the orchestrator checks all six final inventory artifact paths as well as its two status files; an existing artifact blocks the run unless --overwrite is explicit. The tool performs local read-only inspection and does not upload or modify game binaries.

## Validation boundary

Host tests mock Ghidra and the ELF inspector to validate gate ordering, OPD range handoff, missing-input behavior and output status. The orchestrator does not decrypt SELF files. A successful Ghidra inventory is static evidence only and is not a playable Campaign co-op build.
