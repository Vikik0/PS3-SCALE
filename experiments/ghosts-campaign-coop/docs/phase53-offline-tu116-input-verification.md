# Phase 53 — Offline verification of local TU 1.16 inputs

## Purpose

The native PPU analysis must use the exact BLUS31270 disc and TU 1.16 update pair. File names alone are not enough; a second region, an older update, or a modified SELF could invalidate all later address and function comparisons.

## Changes

- Added tools/verify_ghosts_tu116_inputs.py.
- It locates exactly one disc PARAM.SFO and installed-update PARAM.SFO, validates Title ID, category and app version, and checks the known TU 1.16 update SFO hash.
- It locates EBOOT.BIN, default.self and default_mp.self in the installed update and checks their exact size, SHA-256, SELF key revision/program type, PPU ELF64 big-endian identity, entry points, program-header count and encrypted segment indices.
- Optional base FastFiles are hash-checked if present. Missing optional FastFiles do not invalidate the retail SELF profile.
- Optional decrypted Campaign/Multiplayer ELFs are checked for the expected entry points and PPU ELF layout. Both must be present and distinct before the report marks native PPU comparison ready.
- The tool writes metadata-only JSON. It never decrypts, patches, copies or uploads retail game files.

## Local invocation

    python tools/verify_ghosts_tu116_inputs.py --disc-root "D:\Games\Ghosts\PS3_GAME" --update-root "D:\RPCS3\dev_hdd0\game\BLUS31270" --output analysis\ghosts-tu116-inputs.json

Optional decrypted ELF paths can be added with --campaign-elf and --multiplayer-elf. The report distinguishes verified encrypted SELF containers from actual decrypted PPU ELF availability.

## Validation boundary

Host tests use synthetic SFO and SELF fixtures with the expected profile constants patched to fixture hashes. A successful local SELF identity check does not decrypt its executable segments and does not by itself enable Ghidra function analysis.
