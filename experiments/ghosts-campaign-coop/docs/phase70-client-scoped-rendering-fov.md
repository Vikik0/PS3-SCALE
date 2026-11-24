# Phase 70 — Trace per-client visibility and FOV semantics

## Purpose

Phase 68 identified the strongest clues: two FOV DVAR slots on playerpiggyback entries and separate level.player / level.playengineeffects selectors in the Carrier dog-drone animation. Phase 70 makes those relationships explicit and searchable across the supplied mission scripts.

## Changes

- Add tools/audit_split_screen_client_semantics.py.
- Trace showonclient and hideonclient calls with the receiver, client argument and a conservative client-role classification.
- Map playerpiggyback array indexes to cg_playerFovScale0 / cg_playerFovScale1 assignments.
- Trace setsaveddvar calls using self.sniper_degy.
- Find comparisons between self.favoriteenemy and level.playengineeffects.
- Hash source trees before and after scanning.

## Windows usage

~~~powershell
python tools/audit_split_screen_client_semantics.py black_ice="D:\Ghosts\Phase31\missions\black_ice" carrier="D:\Ghosts\Phase31\missions\carrier" cornered="D:\Ghosts\Phase31\missions\cornered" --output-json "D:\Ghosts\analysis\phase70-client-semantics.json"
~~~

## Interpretation boundary

The scanner can show how scripts distinguish the primary player handle from a secondary-client candidate for visibility and FOV. It cannot identify the native initializer for that candidate or prove that two local controllers are active.
