# Phase 69 — Distinguish player-container initialization from reads

## Purpose

Phase 68 found 19 playerpiggyback references and 4 playengineeffects references in the uploaded mission GSC source set. This phase adds a dedicated scanner to determine whether those globals are actually initialized or mutated by the supplied mission scripts.

## Changes

- Add tools/audit_player_container_globals.py.
- Scan all nested .gsc files under each mission root, including both decompiled_gsc and raw_gsc_sources.
- Classify global assignments, array-slot assignments, array mutators, per-player entity-property writes, iteration reads, indexed reads and ordinary references.
- Track level.playengineeffects assignments separately from reads.
- Hash the complete GSC source tree before and after scanning and reject changes.
- Report NO_SCRIPT_INITIALIZER_IN_SCANNED_GSC when no script-side assignment is present; do not infer native initialization from absence.

## Windows usage

~~~powershell
python tools/audit_player_container_globals.py black_ice="D:\Ghosts\Phase31\missions\black_ice" carrier="D:\Ghosts\Phase31\missions\carrier" cornered="D:\Ghosts\Phase31\missions\cornered" --output-json "D:\Ghosts\analysis\phase69-player-container-global-audit.json"
~~~

## Validation boundary

Synthetic tests distinguish writes from reads and confirm nested raw/decompiled GSC files are included. Absence of an initializer in the supplied mission scripts does not prove that the engine never initializes the globals.
