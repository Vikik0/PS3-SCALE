# TU 1.16 native split-screen script map

## Scope

This report is based on the named/decompiled GSC files extracted from the two user-supplied TU 1.16 patch FastFiles:

- `patch_common.ff`: 2 ScriptFiles.
- `patch_common_mp.ff`: 119 ScriptFiles.
- Combined named script tree audited: 121 GSC files, 4,240 top-level functions.

A function-level scan found 35 split-screen predicate calls across 15 functions in 7 files. It also found 280 references to `level.playerpiggyback` and 76 direct `level.player` references across the 121-file patch tree. The numeric decompiler outputs were excluded from the count to avoid double-counting the renamed files.

These are Multiplayer/shared patch scripts. Their existence proves the TU 1.16 script layer contains split-screen handling; it does not prove Campaign starts a second local client.

## Function map

| Script | Function | Split-screen checks | Observed responsibility |
|---|---|---:|---|
| `maps/_load.gsc` | `magicreloadwhenreachenemy` | 1 | Calls player registration and sets saved `cg_fovScale` to 0.75 for split-screen, otherwise 1.0. |
| `maps/mp/_utility.gsc` | `_beginlocationselection` | 1 | Uses smaller font and different camera-relative offset for split-screen loadout/location UI. |
| `maps/mp/_utility.gsc` | `playsoundinspace` | 8 | Iterates the registered player array and skips the secondary local split-screen player for local sounds/dialog routing, avoiding duplicate playback. |
| `maps/mp/_utility.gsc` | `showfakeloadout` | 1 | Suppresses the fake loadout in a specific Horde/split-screen condition. |
| `maps/mp/gametypes/_hud_message.gsc` | `initnodegraph` | 1 | Uses split-screen-specific font sizing and placement for per-player notification HUD. |
| `maps/mp/gametypes/_hud_message.gsc` | `shownoshow` | 1 | Applies split-screen-specific HUD icon sizing. |
| `maps/mp/gametypes/_hud_message.gsc` | `lowermessages` | 1 | Adjusts lower-message font size and vertical placement for split-screen. |
| `maps/mp/gametypes/_hud_util.gsc` | `createprimaryprogressbar` | 1 | Offsets the progress bar for a split-screen player. |
| `maps/mp/gametypes/_hud_util.gsc` | `createprimaryprogressbartext` | 1 | Offsets progress-bar text for a split-screen player. |
| `maps/mp/gametypes/_music_and_dialog.gsc` | `dointro` | 8 | Only the primary local player receives certain spawn music and leader-dialog playback. |
| `maps/mp/gametypes/_music_and_dialog.gsc` | `onrotatingvehicleturret` | 6 | Iterates local players for round/side-switch dialog but skips the secondary split-screen player. |
| `maps/mp/gametypes/_music_and_dialog.gsc` | `onfreeplayerconnect` | 2 | Applies a depth-of-field effect only when the player is not a split-screen player. |
| `maps/mp/gametypes/_playerlogic.gsc` | `in_range_for_homing_missile_sqrd` | 1 | Avoids the normal single-view depth-of-field effect in split-screen. |
| `maps/mp/gametypes/_playerlogic.gsc` | `initclientdvars` | 1 | Sets per-client grenade HUD dimensions and `cg_fovscale=0.75` for split-screen players. |
| `maps/mp/gametypes/_rank.gsc` | `onplayer` | 1 | Stores the player's `isSplitscreen` state in match data under that player's client ID. |

## Native player registration and lifecycle

The key registration path is separate from the 35 display/behavior checks:

1. `maps/_load.gsc::init_level_flags()` collects existing player entities into `level.playerpiggyback`, assigns the first to `level.player`, and assigns the second to `level.playengineeffects` when present.
2. `maps/mp/gametypes/_playerlogic.gsc::callback_playerconnect()` waits for the engine-provided player's `begin` event, then registers that entity in `level.playerpiggyback`.
3. `initclientdvars()` checks the native per-player split-screen predicate and sets client-specific HUD/FOV values.
4. Disconnect cleanup removes the entity from the registered player array.
5. The GSC `spawnPlayer()` path is a respawn routine for an existing player object. It is not a native local-client creation function.

The split-screen predicates therefore operate on player entities that the engine has already created and associated with local-client slots. The script code does not reveal how the engine allocates local client 1 or maps controller port 1 to that slot.

## Implications for Campaign

- The shared loader can retain a second player entity **if the native engine has already created it**.
- Multiplayer contains a clear per-client lifecycle and presentation path that Campaign can use as a reference.
- The secondary split-screen player is intentionally excluded from some audio/dialog work, while each player's HUD and FOV are adjusted separately.
- Campaign's Clockwork mission scripts still contain 644 direct `level.player` references and no split-screen predicate calls. The MP support does not automatically make those mission scripts two-player aware.
- The new `ghc_campaign_player_registry.gsc` shim checks the existing player list and native primary/secondary predicates, but it cannot create a player and has no verified injection path yet.

## Next executable-level question

Identify the PPU-side function chain that creates local client 1, initializes its player state and command stream, assigns controller port 1, and creates the second viewport. The TU 1.16 SELF files received so far are encrypted containers; without a verified decrypted ELF or equivalent exact-build symbol map, no hook address is approved.

Do not use a GSC player respawn routine, AI clone, or arbitrary `G_Spawn` call as a substitute for this native local-client initialization path.
