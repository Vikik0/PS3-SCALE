# Clockwork Campaign player-dependency audit

## Scope and provenance

This is a structured, function-level audit of the user-supplied `clockwork.ff` sample, not a claim that every Campaign mission or the TU 1.16 mission asset set has been inspected.

- Input: `clockwork.ff`
- Size: 85,855,599 bytes
- SHA-256: `a2ac70c7349047f2eded5f45a5e46ff8278689839fe1655106263520fe639783`
- Extracted mission scripts: the 11 `maps__clockwork*.gsc` files.
- The sample's exact Title Update provenance is not established by this hash. The TU 1.16 `patch_common.ff` and `patch_common_mp.ff` are separate inputs.

The audit utility is `tools/campaign_player_dependency_audit.py`. It parses top-level function boundaries, strips comments and string literals before counting tokens, and records function/line references without copying GSC source text into its report.

## Results

| Measure | Result |
|---|---:|
| Clockwork mission GSC files | 11 |
| Top-level functions | 988 |
| Functions containing direct `level.player` references | 205 |
| Direct `level.player` references | 644 |
| `level.playerpiggyback` references | 1 |
| Split-screen predicate references in mission files | 0 |
| `coop` references in mission files | 0 |

### Direct player references by mission script

| Script | `level.player` references |
|---|---:|
| `maps__clockwork.gsc` | 1 |
| `maps__clockwork_anim.gsc` | 0 |
| `maps__clockwork_audio.gsc` | 72 |
| `maps__clockwork_code.gsc` | 115 |
| `maps__clockwork_defend.gsc` | 84 |
| `maps__clockwork_exfil.gsc` | 117 |
| `maps__clockwork_interior.gsc` | 118 |
| `maps__clockwork_interior_nvg.gsc` | 23 |
| `maps__clockwork_intro.gsc` | 112 |
| `maps__clockwork_pip.gsc` | 2 |
| `maps__clockwork_scriptedlights.gsc` | 0 |

The single `playerpiggyback` reference in the mission tree is in the Clockwork entry script, where the complete player array is passed to the night-vision initializer. It is not a per-player Campaign interaction layer.

### Highest-impact functions

| Script | Function | Direct player references |
|---|---|---:|
| `maps__clockwork_interior.gsc` | `handle_drill_spot` | 32 |
| `maps__clockwork_code.gsc` | `hold_count_check` | 31 |
| `maps__clockwork_interior.gsc` | `new_trench_setup` | 20 |
| `maps__clockwork_intro.gsc` | `intro_door` | 16 |
| `maps__clockwork_interior.gsc` | `handle_misc_drill_details` | 15 |
| `maps__clockwork_exfil.gsc` | `in_spawnspectator` | 14 |
| `maps__clockwork_intro.gsc` | `player_bob_scale_set` | 14 |
| `maps__clockwork_defend.gsc` | `listen_for_use_proximity_duffle_bag` | 13 |
| `maps__clockwork_defend.gsc` | `listen_for_use_multi_turret` | 13 |
| `maps__clockwork_exfil.gsc` | `new_l_pip_corner` | 13 |

### Common player API touchpoints

The line-count report shows repeated direct calls through `level.player` for:

- Audio and feedback: `playsound` (60 lines), `playrumbleonentity`.
- Equipment: `giveweapon` (24), `takeweapon` (20), `switchtoweapon` (15), `getcurrentweapon` (14).
- Mission UI/actions: `setactionslot` (20), `notifyonplayercommand` (11).
- Player input: `attackbuttonpressed` (13), `adsbuttonpressed` (6), `meleebuttonpressed` (1).
- Scripted movement: `playerlinktoblend` (8), `playerlinktodelta`, `setplayerangles`, `setorigin`.
- Control gating: `freezecontrols`, `allowcrouch`, `allowprone`, `allowsprint`, `allowjump`.

These are line counts of direct `level.player <method>(...)` calls, not unique method definitions. Category totals in the JSON output are intentionally non-exclusive.

## What the shared scripts already provide

The following are findings from the inspected shared files, with version boundaries kept explicit.

1. **TU 1.16 `patch_common.ff` — `maps/_load.gsc`:** `init_level_flags()` is present and unchanged from the base common sample. It collects already-existing `player` entities with `getentarray("player", "classname")`, assigns index 0 to `level.player`, and stores index 1 as `level.playengineeffects` when a second entity already exists. This function registers existing entities; it does not allocate a local client or create the second player.
2. **Base shared `maps/_utility.gsc`:** `get_other_player()` resolves the other entity from `level.playerpiggyback`. `is_controlling_uav()` returns true for `issplitscreen()` or when the `coop` dvar equals `"1"`. `is_coop()` explicitly returns false for split-screen, then identifies the separate `coop`-dvar mode. Do not treat these two predicates as interchangeable.
3. **Base shared `maps/_coop.gsc`:** the module contains partner-location HUD setup and reacts to weapon-fire, damage and downed-state changes. It does not spawn a second player. No direct call to this module's initializer was found in the 11 Clockwork mission scripts.
4. **Base shared `maps/_gameskill.gsc`:** `coop_with_one_player_downed()` checks whether one healthy player remains in the controlling-UAV/co-op context. Its caller adjusts difficulty settings through `make_player_look_away()`; this is not a teammate revive implementation.
5. **TU 1.16 `patch_common_mp.ff` — `maps/mp/gametypes/_playerlogic.gsc`:** `callback_playerconnect()` waits for the engine-provided player entity's `begin` event, then appends that entity to `level.playerpiggyback`. `initclientdvars()` uses the native `issplitscreenplayer()` predicate to apply split-screen HUD/FOV values. Disconnect removes the entity from the list. This is a post-creation lifecycle, not a GSC routine that creates local client 1.

## Engineering conclusion

The engine/shared-script layer has data structures and predicates for more than one player entity. That does **not** mean the Campaign mission scripts are automatically two-player aware.

- Clockwork's mission-specific code hard-codes the primary `level.player` in 205 functions.
- The mission tree contains no explicit split-screen or `coop` branch.
- Setting a dvar or showing a Mod Menu cannot, by itself, create a native second local client or route controller 2 into Campaign.
- A spawned AI soldier is not equivalent to a second local player: the second slot needs its own client identity, controller command stream, player state, camera, viewport and HUD ownership.
- The next critical gate is still the exact TU 1.16 native local-client creation/initialization path. The uploaded retail SELF containers expose metadata but not decrypted PPU instructions. No unverified offset should be hooked.

## Adaptation order after the native client gate

1. Prove two native local player slots exist in a supported offline mode and identify the primary/secondary identity and controller-port mapping.
2. Prove Campaign startup can retain those slots without replacing story NPCs.
3. Refactor player-specific mission operations rather than globally replacing every `level.player` token. Shared mission flags and global scripted events must remain shared; weapon, input, transform, view, HUD and interaction handlers must become player-scoped.
4. Start with Clockwork's highest-impact functions: vault/drill interaction, equipment setup, intro scripted movement, defend pickups/turrets and exfil/vehicle interactions.
5. Add independent camera/HUD, death/downed/revive and checkpoint behavior, then test mission transitions.
6. Repeat the audit across the remaining Campaign mission FastFiles before claiming full-campaign coverage.

## Current boundary

This audit is complete for the available Clockwork sample only. It is not a playable mod, does not change any retail file, and does not establish that Campaign can enter the native split-screen path. The FastFile signing constraint and executable-analysis gate remain in force.
