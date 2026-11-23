# Phase 31 — Black Ice, Carrier and Cornered Campaign audit

## Extraction and validation

The supplied files are IW6 PS3 FastFiles with outer magic IWff0100 and version 0x22E. Their SHA-256 values are recorded in PHASE31_MISSION_AUDIT_SUMMARY.json.

GSC Tool 1.6.2.364 extracted 249 compiled GSC candidates:
- Black Ice: 76
- Carrier: 118
- Cornered: 55

All 249 candidates decompiled successfully. All 249 resulting source files also compiled successfully with the same pinned GSC Tool. Three source GSC RawFile assets were preserved separately.

## Player dependency audit

| Mission | Map GSC files | Map functions | Functions with level.player | Direct level.player references |
|---|---:|---:|---:|---:|
| Black Ice | 16 | 966 | 203 | 569 |
| Carrier | 21 | 923 | 195 | 596 |
| Cornered | 18 | 1,109 | 240 | 1,090 |
| Total | 55 | 2,998 | 638 | 2,255 |

Across all 249 extracted GSC files, including AI and support scripts, the audit found 4,304 functions, 685 functions with direct level.player references, and 2,371 direct references.

The audit strips comments and string literals before counting. It is a static inventory, not a runtime capability test.

## Existing multi-player-aware paths

### Black Ice
1062.gsc::player_saw_kill() checks issplitscreen(). In split-screen it starts player_scuba() only when self == level.player; the breath-sound thread is started separately. This is a narrow primary-player gate, not general Player 2 support. No level.playerpiggyback or level.playengineeffects references were found in the extracted Black Ice scripts.

### Carrier
- 1031.gsc::monitorscavengerpickup() iterates level.playerpiggyback, creates HUD state per player, and selects cg_playerFovScale0 / cg_playerFovScale1 when there are two players. No direct call site for this function was found in the extracted Carrier GSC set, so it is an existing helper candidate rather than a confirmed active mission path.
- maps/ss_util.gsc::get_different_favoriteenemy() selects the other player from level.playerpiggyback using level.playengineeffects. Its dog-monitor parent function was not found at an external call site in the extracted set, so this target-switching path also remains unconfirmed.
- dog_combat.gsc::playerview_playmissanim() has an in-file caller and uses hideonclient() with level.playengineeffects and the primary player in a dog-animation path. This is a confirmed player-aware code path, not proof of full cooperative gameplay.

### Cornered
Mission-called functions courtyard_rorke() and bar_enemies() iterate level.playerpiggyback to apply stealth/visibility behavior to both player entities. Additional AI support scripts select threat targets from the player array or pass the array to array_thread(). This is stronger evidence of existing multi-player-aware mission logic, but many mission event functions still hard-code level.player.

## Highest-priority mission functions

- Black Ice: player_fail_finale_instakill (26 refs), ascend_logic (22), event_player_turns_key (17).
- Carrier: knockback_hesh (51), cut_the_rope (22), victory_choppers_land (20).
- Cornered: gun_down_trigger_internal (78), fall_player (47), upload_virus_enter (44).

These are review priorities only. Do not mechanically replace every level.player: mission-wide flags, dialogue, scripted world events and checkpoint state must remain shared where appropriate.

## MapEnts boundary

The extracted zones reference maps/black_ice.d3dbsp, maps/carrier.d3dbsp, and maps/cornered.d3dbsp, each with one MapEnts asset-pool record. The current candidate scanner did not find a valid inline entity-string payload in these FastFiles. Spawn locations and map entity coordinates therefore remain unverified; the corresponding .d3dbsp files are the next required map-data inputs if available.

## Container boundary

The current outer FastFile parser did not account for every byte in the three containers:
- Black Ice: 2,048 validated 64 KiB blocks (128 MiB) followed by unclassified data.
- Carrier: 2,048 validated 64 KiB blocks (128 MiB) followed by unclassified data.
- Cornered: 499 validated blocks and a zero terminator, followed by a large unclassified trailing region.

The successful GSC extraction and compilation applies to the validated ScriptFile assets found before those unresolved regions. It does not mean the entire FastFiles were fully decoded or rebuilt.

## CI validation

GitHub Actions run 45206287037 passed all five jobs. The Python suite passed 284 tests; the pinned GSC Tool compile/decompile job, protocol checks, worker bundle and PS3 PRX bootstrap build also passed.

## Engineering conclusion

Carrier and Cornered contain reusable player-array-aware logic, with some Carrier helper call paths still unconfirmed. Black Ice contains only a narrow split-screen check in the extracted mission/support scripts. All three still have substantial direct primary-player coupling.

The next safe work is to resolve the map-data files and trace each high-impact function's call sites and shared-state effects. A native second Campaign local-client slot, controller routing, player state, camera and HUD must be proven separately; GSC edits alone cannot create that engine-side client.
