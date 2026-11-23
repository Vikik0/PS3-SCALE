# Phase 46 — Bind OPD seeding to the exact ELF inspector report

## Purpose

Copying an OPD address and size manually from an inspector report into the Ghidra command creates a risk of using a stale report or mixing Campaign and Multiplayer ranges. The runner now accepts the inspector JSON itself and extracts the range only after provenance and structural validation.

## Changes

- Added --campaign-opd-inspector-report and --multiplayer-opd-inspector-report.
- The runner accepts only the supported ps3_elf_inspect 0.1.2 report format for the declared BLUS31270 / TU 1.16 / CEX 4.93 profile.
- The report's input SHA-256 must match the exact Campaign or Multiplayer ELF passed to Ghidra.
- The OPD candidate must match its containing section and pass the inspector's alignment, readability, descriptor-slot, code-pointer, TOC and trailing-byte checks.
- Manual OPD address/size remains supported, but cannot be combined with an inspector report for the same build. The manifest labels manual ranges as operator-declared.
- The runner hashes both ELF inputs before analysis and checks them again before promotion. If either file changes while Ghidra runs, no final output is promoted.
- The manifest records the inspector report path/hash, input ELF hash, section identity and the source of the OPD range.

## Validation boundary

Synthetic reports and mocked Ghidra runs verify exact-hash matching, candidate validation, CLI range selection, mutual exclusion and input-change rejection. CI still does not execute Ghidra. Actual use requires the matching decrypted TU 1.16 PPU ELF and its own inspector report for each build.
