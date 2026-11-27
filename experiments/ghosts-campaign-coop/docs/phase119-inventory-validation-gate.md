# Phase 119 — Phase 118 inventory validation gate

## Purpose

Phase 118 pinned a static inventory of the original Campaign mission trigger calls. This phase makes that evidence self-checking when copied into the PS3 development and capture kit.

## What is validated

- The source archive identity and SHA-256 format.
- Exactly 15 call sites, distributed as 3 Black Ice, 2 Carrier and 10 Cornered.
- Positive source line numbers, non-empty callers, stable helper names and consistent per-file source hashes.
- Four critical interactions with stable Event Lock IDs 0–3.
- The Carrier rope-cut path remains classified as notifyonplayercommand.
- All original handlers remain unmigrated and all four critical interactions remain blocked until native Campaign Player 2 is proven.
- The report and machine-readable validation summary agree on the archive hash, call-site counts and runtime boundary.

## Run the validator

From the Ghosts project root:

~~~bash
python tools/verify_phase118_trigger_inventory.py \
  --inventory reports/PHASE118_TRIGGER_CALLSITE_INVENTORY.json \
  --validation reports/PHASE118_VALIDATION.json \
  --report reports/PHASE118_REPORT_AR.md
~~~

The capture-kit verifier also runs this check against the packaged copies. The validator is copied into Tools with the three Phase 118 evidence files and the call-site inventory document.

## Packaging correction

The capture-kit builder previously copied the Arabic runtime quick-start twice to the same ZIP path. The duplicate copy was removed, and a regression test now requires exactly one quick-start entry.

## Runtime boundary

This is a static evidence and package-integrity improvement only. It does not create a Local Client, spawn Player 2, add camera/HUD state, or make a Campaign mission playable. No mission source, EBOOT, SELF or retail FastFile is modified. Player 2 remains NOT_PROVEN, and shared interactions remain 0/4 until the actual BLUS31270 TU 1.16 Campaign/Multiplayer runtime captures and controller logs are analyzed.
