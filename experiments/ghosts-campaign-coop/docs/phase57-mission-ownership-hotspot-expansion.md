# Phase 57 — Expand mission player-ownership hotspots with exact source identity

## Scope

The Phase 31 audit of the three supplied mission FastFiles identifies the top 20 functions with the most direct level.player references in each mission. Phase 33 previously reviewed 17 selected functions. Phase 57 expands the ownership review to all 60 Phase 31 top-20 hotspots plus eight previously selected special-case functions, for 68 exact file/function targets.

## Correctness fix

Some function names occur in more than one decompiled ScriptFile. A name-only target selector can include unrelated copies, especially generic helper names. The default audit configuration now stores each target as a relative GSC path plus function name. The audit reports missing targets as path::function and does not silently substitute a same-name function from another file.

Custom target_names passed by tests or callers retain their previous name-only behavior.

## Verified source coverage

The local Phase 31 extracted bundle contains 249 decompiled GSC files and 4,304 parsed functions. The exact target audit found all 68 configured targets:

- Black Ice: 21 targets, 225 level.player references.
- Carrier: 23 targets, 270 level.player references.
- Cornered: 24 targets, 466 level.player references.

Total: 68/68 target definitions found, with 961 direct level.player references across those selected functions.

## Ownership signals

- Most selected functions mix direct player actions with level-scope fields or mission flags; these require splitting player-specific operations from shared mission progression.
- Carrier contains an explicit secondary-client reference path in playerview_playmissanim and a playerpiggyback-aware path in monitorscavengerpickup.
- Cornered contains playerpiggyback-aware functions including courtyard_rorke, bar_enemies, enemy_threat_logic_dog_wait and enemy_corpse_alert_level.
- These are existing script patterns, not proof that Campaign creates a second native local client or binds Controller 2.

## Source provenance

The analysis uses the user-supplied Phase 31 extraction bundle for:
- black_ice.ff — SHA-256 950726399e16f55fd565438936ec59231649940c89763a87c805409536837532
- carrier.ff — SHA-256 e3a616545f18a7dcae006b4e32264c56b01d627650ea574f3fe710e02543caee
- cornered.ff — SHA-256 1bcb1da55d7e5a893de2a513e6df9b5cd3786e4d642dc76e090ea551cdf39395

## Validation boundary

The 68-target run is a read-only static audit of decompiled GSC. The category labels are heuristic ownership hints. No mission script is rewritten, no retail FastFile is rebuilt, and no second local Campaign player is claimed.
