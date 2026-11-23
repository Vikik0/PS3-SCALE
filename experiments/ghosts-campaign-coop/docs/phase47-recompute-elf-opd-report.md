# Phase 47 — Recompute inspector OPD evidence from the exact ELF

## Problem

Phase 46 bound the report's input SHA-256 to the ELF and checked the report's internal counts. A user-edited JSON could still retain the correct input hash while changing its OPD section fields or descriptor counts. The report hash would faithfully preserve the edited file, but it would not prove that the claims came from the ELF.

## Changes

- The Ghidra runner now parses the exact supplied ELF through PpuElf before accepting an inspector report.
- It verifies the reported PPC64 machine against the parsed ELF.
- It requires the report's complete OPD object, including section identity, range candidate, descriptor list, slot counts, code-pointer validity and TOC evidence, to equal a fresh OPD parse of the same ELF bytes.
- It independently checks the declared section alignment and candidate section identity before accepting the range.
- Tests now generate the inspector report through the actual inspect_file implementation using a synthetic PPC64 ELF fixture, rather than hand-authoring a report-shaped object.
- A tampered candidate range is rejected before Ghidra starts.

## Validation boundary

This verifies the consistency of the local report with the exact ELF bytes. It does not authenticate the game update or prove the operator-declared title/firmware profile. CI uses synthetic ELF data and a mocked Ghidra process; no retail PPU file is analyzed.
