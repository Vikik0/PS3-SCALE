# Phase 8 — GSC split-screen and multi-player script audit

## Scope and method

Using the official GSC Tool 1.6.2 Linux x86-64 build, the locally extracted user-owned script assets were decompiled with the IW6 / PS3 profile. No retail scripts or game binaries are committed to this repository.

| Source FastFile | Extracted ScriptFile candidates | Decompiled |
|---|---:|---:|
| common.ff | 150 | 150 / 150 |
| clockwork.ff | 109 | 109 / 109 |
| patch_common.ff | 2 | 2 / 2 |
| patch_common_mp.ff | 119 | 119 / 119 |
| **Total** | **380** | **380 / 380** |

A reusable scanner is now in tools/audit_gsc_splitscreen.py. It accepts folders of decompiled GSC source and reports split-screen, local-player-array, client-ID, co-op, camera, and per-client visibility references. The actual decompiled scripts remain local and are not stored in the repository.

## 1. Strong evidence of a multi-player script model in shared code

### common asset 1002

The init_level_flags function builds level.playerpiggyback from getentarray("player", "classname"), assigns the first entity to level.player, and, when a second entity exists, assigns it to level.playengineeffects. It also initializes a per-player identifier for every entity.

Reference: common/1002.gsc, lines 1430–1450.

This proves the shared script layer expects a collection that can contain at least two player entities. It does **not** prove that Campaign's native loader creates the second entity.

### common asset 1063

The shared helpers distinguish local split-screen from the separate coop dvar:
- is_controlling_uav returns true when issplitscreen() is true or the coop dvar equals 1.
- is_coop explicitly returns false for split-screen and otherwise follows is_controlling_uav.

Reference: common/1063.gsc, lines 16–33.

The same asset contains teleport_player_post_osprey. It searches spawn entities for script_noteworthy values player1 and player2, then places level.player and level.playengineeffects at the corresponding origins and angles by iterating level.playerpiggyback.

Reference: common/1063.gsc, lines 6626–6662.

This is a concrete two-player placement path in shared GSC. A static caller for teleport_player_post_osprey was not found among the 380 decompiled scripts, so its runtime use in the target Campaign mission remains unconfirmed. 

Other shared assets use the second-player handle in gameplay systems, not just placement:
- common asset 981 updates gameskill for every entity in level.playerpiggyback, then combines level.player.gameskill with level.playengineeffects.gameskill when is_controlling_uav is true (common/981.gsc, lines 305–331).
- common asset 954 makes the autosave/attach sequence end when level.playengineeffects dies in the split-screen/co-op path (common/954.gsc, lines 233–244).
- common asset 1120 creates a second missile attractor for level.playengineeffects when is_controlling_uav is true (common/1120.gsc, lines 82–105).

These are stronger signs of an existing two-player gameplay model in the shared Campaign-era script layer. They still depend on level.playengineeffects already being initialized; they do not show the native routine that creates that player entity/client.

### common asset 962

magicreloadwhenreachenemy checks is_controlling_uav, sets the coop_game flag, and iterates level.playerpiggyback to initialize friendly HUD/icon threads for each player entity.

Reference: common/962.gsc, lines 4–30 and 163–225.

## 2. Clockwork mission-specific evidence

The Clockwork dog combat script calls maps/_utility::is_controlling_uav. When level.playengineeffects is defined, it uses hideonclient to apply the dog/player-double visibility state separately for level.player and the second player reference.

Reference: clockwork/animscripts/dog/dog_combat.gsc, lines 1608–1633.

Across the 109 decompiled Clockwork scripts, the static scan found:
- 7 level.playerpiggyback references in 4 files;
- 3 level.playengineeffects references in 1 file;
- 1 co-op/split-screen helper reference in 1 file;
- no direct issplitscreenplayer or issplitscreenplayerprimary calls.

This suggests Clockwork's mission scripts contain some multi-player-aware handling, but the mission does not itself expose the native player-two creation path.

## 3. Multiplayer implementation references

### Secondary-player identity

The Multiplayer patch uses the native GSC built-ins issplitscreenplayer() and issplitscreenplayerprimary() to distinguish the secondary local player from the primary player. Several connect, intro, round-switch, and victory-dialog paths skip duplicate announcements for the secondary player.

Reference: patch_common_mp asset 1495, lines 205–239, 276–305, and 343–355.

### Split-screen HUD layout

The Multiplayer patch adjusts HUD text size and calls setplayertocamera when issplitscreenplayer() is true.

Reference: patch_common_mp asset 1385, lines 26–44.

### Client IDs are not local-client slots

The Multiplayer patch's getplayerfromclientnum helper compares the argument against getentitynumber() for entities in level.playerpiggyback. The separate clientid allocation code assigns match-data player IDs from 0 through 29.

Reference: patch_common_mp asset 1500, lines 800–812, 1124–1147, and 1199–1215.

Do not mistake this clientid or entity-number lookup for the engine's local-client index. It is match/player bookkeeping, not proof of controller 0/1 mapping.

### Split-screen menus

The Multiplayer patch selects menu asset names suffixed with _splitscreen and precaches split-screen class/control menus.

Reference: patch_common_mp asset 1493, lines 55–69.

## 4. Scan counts

The scanner found the following occurrences in the decompiled source sets:

| Source | level.player | issplitscreen | issplitscreenplayer | issplitscreenplayerprimary | playerpiggyback | playengineeffects | co-op references |
|---|---:|---:|---:|---:|---:|---:|---:|
| common | 442 | 5 | 0 | 0 | 111 | 8 | 22 |
| clockwork | 789 | 0 | 0 | 0 | 7 | 3 | 1 |
| patch_common | 50 | 1 | 0 | 0 | 15 | 1 | 2 |
| patch_common_mp | 26 | 1 | 21 | 12 | 265 | 0 | 0 |

Counts are static textual occurrences, not runtime call counts. "co-op references" includes the coop dvar, is_coop/is_controlling_uav helpers, and coop_game flags.

## 5. Engineering conclusion

The GSC layer is more prepared for two-player operation than the initial static analysis suggested:

1. Shared code stores a first and second player entity separately.
2. Shared code includes player1/player2 spawn-marker placement.
3. Shared code and Clockwork mission logic contain per-client visibility and co-op HUD handling.
4. Multiplayer has native split-screen identity checks, camera-relative HUD layout, and split-screen menu variants.

However, GSC scripts consume player entities and native split-screen state; they do not demonstrate how the engine creates a second local client. No script-side local-client creation routine was identified. The critical unresolved step remains the native Campaign path that creates/activates local-client slot 1 and binds it to a separate player entity, input command stream, camera, and HUD.

The coop dvar is not a substitute for issplitscreen. Do not force coop=1 or copy Multiplayer hooks into Campaign based on these GSC findings alone.

## 6. Next checks

1. Use the Phase 8 PS3MAPI runtime captures to compare Campaign and Multiplayer PPU code at the same virtual addresses.
2. Identify the native routine that backs issplitscreen, issplitscreenplayer, and issplitscreenplayerprimary.
3. Trace local-client creation, controller-port mapping, client/player-state allocation, and view/refdef setup in the verified TU 1.16 runtime ELF.
4. Only after the native second-client path is proven, test the shared GSC two-player paths and the Clockwork mission's per-client behavior.

**Status:** GSC decompilation and static audit complete for 380/380 extracted ScriptFile candidates. Native Campaign local-client creation remains unproven; no hook or playable co-op build is claimed.


## 7. Clockwork single-player coupling

A separate scan of exact primary-player references found **789 occurrences of level.player across 26 Clockwork scripts**, compared with only 7 level.playerpiggyback references across 4 scripts and 3 level.playengineeffects references in one script.

The largest primary-player reference clusters are:

| Clockwork script | level.player occurrences |
|---|---:|
| maps/clockwork_interior.gsc | 118 |
| maps/clockwork_exfil.gsc | 117 |
| maps/clockwork_code.gsc | 115 |
| maps/clockwork_intro.gsc | 112 |
| maps/clockwork_defend.gsc | 84 |
| maps/clockwork_audio.gsc | 72 |
| maps/ally_attack_dog.gsc | 34 |

Clockwork asset 1218 also initializes level.player directly from the first entity returned by getentarray("player", "classname") (clockwork/1218.gsc, lines 4–9).

This is the main mission-level integration risk. The shared layer has a second-player handle, but much of Clockwork's story/gameplay logic addresses the primary player explicitly. That does not mean every occurrence must be duplicated: some are intentionally host/leader-only, some are camera/cutscene operations, and some are per-player gameplay. Each function must be classified before adapting it.

Do not mechanically replace level.player with level.playerpiggyback. The correct behavior depends on whether the operation is:
- global/mission-level and should execute once;
- player-specific and should run once per local player;
- primary-player-only (story camera, scripted sequence, leader choice);
- client-specific (HUD, visibility, view/camera, audio);
- shared state that must aggregate both players (difficulty, checkpoints, down/death state).

The reusable GSC scanner now reports primary_player_ref separately from playerpiggyback and playengineeffects so this audit can be repeated as scripts are revised.


### Highest-density Clockwork function clusters

A function-level pass over the highest-density scripts gives a first review queue:

| Script | Function | Direct level.player refs | Likely review focus |
|---|---|---:|---|
| maps/clockwork_interior.gsc | handle_drill_spot | 34 | interactive object ownership and player proximity |
| maps/clockwork_exfil.gsc | in_spawnspectator | 25 | spectator/spawn state and per-player view |
| maps/clockwork_code.gsc | hold_count_check | 20 | hold/use interaction and shared objective state |
| maps/clockwork_intro.gsc | player_intro_anim | 18 | scripted animation and synchronization |
| maps/clockwork_defend.gsc | listen_for_use_multi_turret | 16 | turret interaction ownership |
| maps/clockwork_audio.gsc | get_closest_speaker_nodes | 14 | player-relative audio selection |
| maps/ally_attack_dog.gsc | dog_player | 11 | player/dog combat ownership |

These are triage counts, not a final patch list. Each function needs review of its caller, event lifetime, and whether the operation is intentionally tied to the story's primary player.


Clockwork's main map script also passes the entire level.playerpiggyback array into the night-vision initialization helper (maps/clockwork.gsc, line 29), which is explicitly a multi-player-friendly call site. A separate numeric Clockwork script asset 1218 initializes level.player from the first player entity only (1218.gsc, lines 4–9). Together these show mixed handling: some mission systems already accept the player array, while other initialization and gameplay code remains primary-player-centric.
