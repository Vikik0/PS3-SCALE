# Phase 9 — Campaign/Multiplayer GSC player-lifecycle evidence

## Scope and input integrity

This phase is a read-only analysis of the user's locally supplied BLUS31270 GSC exports. The GSC Tool 1.6.2.364 logs report:

| Export set | Decompiled scripts | Result |
|---|---:|---|
| Clockwork Campaign | 109 / 109 | Complete |
| Shared common | 150 / 150 | Complete |
| patch_common overlay | 2 / 2 | Complete |
| patch_common_mp Multiplayer overlay | 119 / 119 | Complete |

No extracted GSC source or retail asset is added to this repository. The report records script IDs, line references, and conclusions only.

## 1. New finding: Campaign's shared script layer already models more than one player entity

The Clockwork map calls the shared load script at clockwork/maps/clockwork.gsc:21. The shared load script calls init_level_flags() at common/1002.gsc:117.

common/1002.gsc:1430-1448 performs the following sequence:

1. Queries existing entities with getentarray("player", "classname").
2. Stores the returned entities in level.playerpiggyback.
3. Assigns level.player to array element 0.
4. If the array contains more than one entity, assigns element 1 to level.playengineeffects.
5. Iterates the player array to start per-player threads.

The same init_level_flags() function in the supplied patch_common/1002.gsc overlay is byte-for-byte identical to the base common/1002.gsc function. The update overlay therefore does not remove this player-array setup. The same load function also checks issplitscreen() at common/1002.gsc:119-122 and changes cg_fovScale to 0.75 for split-screen. A shared health-overlay helper adjusts its layout for split-screen and the primary player at common/981.gsc:1134-1149.

The animation initialization path also calls maps/_load::init_level_flags() from common/animscripts/init.gsc:323-336; Clockwork's dog initialization enters that path at clockwork/animscripts/dog/dog_init.gsc:20.

Clockwork mission code consumes the array at clockwork/maps/clockwork.gsc:29 by passing level.playerpiggyback to the night-vision helper. Its dog view-animation code at clockwork/animscripts/dog/dog_combat.gsc:1612-1628 checks is_controlling_uav() and, when level.playengineeffects exists, uses client-specific hide operations to keep the two player-side view entities separated during the dog knockdown sequence.

**Interpretation:** the Campaign/shared GSC layer is not strictly hard-coded around one player object. It has a primary-player alias and a conditional second-player alias. However, the code only enumerates player entities that already exist; it does not create a second native local client.

## 2. Shared co-op helpers

common/1063.gsc:16-32 defines two distinct checks:

- is_controlling_uav() returns true when issplitscreen() is true or the coop dvar equals 1.
- is_coop() explicitly returns false for split-screen, then returns true for the non-split-screen co-op branch.

Other shared helpers operate over level.playerpiggyback:

- get_closest_player() and get_players_healthy() select from the full player array (common/1063.gsc:815-843).
- get_other_player() returns a different player entity from that array (common/1063.gsc:6760-6768).
- The shared co-op HUD script initializes per-player color and friendly-location HUD elements when is_controlling_uav() is active (common/962.gsc:4-30, 82-116).
- Autosave logic waits for the secondary alias to die when the co-op/split-screen branch is active (common/954.gsc:238-242).
- Missile targeting creates a second attractor for level.playengineeffects when the co-op/split-screen branch is active (common/1120.gsc:99-105).
- Shared difficulty logic compares the primary and secondary players' gameskill values (common/981.gsc:315-326).

The coop dvar is a script-mode condition, not a client-creation command. Setting coop=1 alone does not establish a second local client or bind another controller.

There is also a helper teleport_player_post_osprey() (common/1063.gsc:6626-6662) that maps map entities marked player1 and player2 to level.player and level.playengineeffects. No call site for this helper was found in the supplied decompiled source sets, so it is a dormant lead, not proof that Clockwork uses a second-player teleport path.

## 3. Multiplayer's native script-side connection lifecycle

The Multiplayer overlay contains a clearer connection pipeline:

1. patch_mp/1467.gsc:16-23 is the engine callback wrapper. It dispatches to level.callbackplayerconnect.
2. patch_mp/1467.gsc:100-110 assigns that callback to maps/mp/gametypes/_playerlogic::callback_playerconnect.
3. patch_mp/1505.gsc:4-18 initializes level.playerpiggyback as an empty array during spawn-logic setup.
4. patch_mp/1500.gsc:1171-1304 waits for the player's begin event, assigns a client ID, initializes the player state, then appends self to level.playerpiggyback.
5. patch_mp/1500.gsc:1124-1147 searches the 0–29 client-ID range for an unused ID.
6. patch_mp/1500.gsc:800-811 maps an entity number back to an entry in level.playerpiggyback.
7. patch_mp/1500.gsc:992-1068 handles disconnect and removes the departing entity from the player array.

The Multiplayer scripts also use issplitscreenplayer() and issplitscreenplayerprimary() to distinguish primary and secondary local players for audio/dialogue behavior (patch_mp/1495.gsc:205-240, 275-301, 343-350). patch_mp/1501.gsc:192-195 records the isSplitscreen property for a player.

These are script-side consumers of a split-screen state. The scanned GSC does not show where the engine creates the extra local client, maps controller port 1 to it, or assigns its independent render/view context.

## 4. Quantified source audit

The following counts are from the 378 decompiled files in the three principal source groups. Counts are token/reference occurrences, not unique players or runtime events.

The expanded audit is reproducible against local exports without uploading game scripts:

~~~powershell
python tools/audit_gsc_splitscreen.py --group "campaign=D:\Ghosts\analysis\clockwork_decompiled" --group "common=D:\Ghosts\analysis\common_decompiled" --group "multiplayer=D:\Ghosts\analysis\patch_mp_decompiled" --output "D:\Ghosts\analysis\phase9_gsc_audit.json" --max-hits 5000
~~~


| Group | Files | playerpiggyback refs | playengineeffects refs | Split-screen refs | Other useful markers |
|---|---:|---:|---:|---:|---|
| Clockwork Campaign | 109 | 7 | 3 | 0 per-player split-screen built-ins | 2 player-entity queries; 23 setplayertocamera calls |
| Shared common | 150 | 111 | 8 | 5 issplitscreen() calls | 7 get_other_player; 5 coop_game; 3 each player1/player2 marker |
| Multiplayer patch | 119 | 265 | 0 | 21 issplitscreenplayer; 12 issplitscreenplayerprimary; 33 splitexitsright refs | 106 clientid; 2 connect-callback refs; 2 client-ID allocator refs; 1 getplayerfromclientnum |

A separate scan of the Clockwork and shared sources found no direct spawn("player"), createplayer(), or equivalent GSC call that creates a second player entity. The player list is populated from getentarray; the second alias is conditional on the returned array already containing at least two player entities.

## 5. Engineering conclusion

This changes the approach for Campaign co-op:

- Do not start by inventing a new Campaign GSC player-array system. The shared code already has one.
- The unresolved gate is earlier: determine how the PS3 engine creates/activates a second local client and exposes its player entity to getentarray("player", "classname").
- Then verify that the second entity is bound to controller port 1 and receives an independent view/render context.
- Only after those facts are observed should a Campaign-specific hook or mission adapter be considered.

The current evidence does **not** prove a playable second Campaign player. It does not establish an independent camera, HUD viewport, player-state ownership, spawn/respawn lifecycle, or controller mapping. The player1/player2 teleport helper has no identified caller in the supplied sources.

## 6. Next runtime gates

1. Use Pad Probe v4 in Campaign and Multiplayer. Require a sample where both ports are connected simultaneously (port_mask=0x03).
2. Capture the TU 1.16 Campaign and Multiplayer runtime segments using the separate manual PS3MAPI scripts.
3. Verify both dumps against the exact installed default.self and default_mp.self headers.
4. Compare the runtime ELFs and identify the native local-client creation path, player-entity creation, controller mapping, and independent view contexts.
5. Test the native path in a controlled mission while preserving Logan as player one and Hesh as an NPC.

**Status:** Campaign/shared GSC has conditional second-player entity handling; native Campaign local-client creation and controller-to-player binding remain unproven. No hook address, patched EBOOT, installable PKG, or playable co-op build is claimed.


## 7. Campaign registry shim compile check

The existing read-only Campaign registry shim at client/ps3/gsc/ghc_campaign_player_registry.gsc was independently compiled with GSC Tool 1.6.2.364 for IW6/PS3 and then round-trip decompiled successfully.

- Compiled GSCBIN: 400 bytes.
- SHA-256: 0c52cb5712cadce6308b47c9c216ac7c483036bd3084ff35899c096c6d35ee85.
- The hash and size match the verified Phase 7 registry artifact manifest.
- The shim assigns slot 0/1 by native split-screen primary identity rather than assuming player-array order.
- It fails closed unless exactly two native split-screen player entities are already present.

This verifies the script's syntax and compiled artifact only. It has not been injected into the game or run on PS3 hardware.


## 8. Pad Probe v4 unload-safety update

The diagnostic PRX was hardened so module_stop checks the worker-thread join result. If the worker cannot be joined, the module returns SYS_PRX_STOP_FAIL and remains resident instead of allowing an unload while code may still be executing. A successful join returns SYS_PRX_STOP_OK.

GitHub Actions run 45023069795 passed all five jobs and 124/124 host tests. The PS3DK job built the PRX and passed prx-gen check. The verified artifact is ghosts-campaign-pad-probe (artifact ID 14754149725), 101,363 bytes, SHA-256 d8739f2b345ddadcfb154b89fa7baacf7b5934d1728e2752cb0c3bfcbc2b960e.

Contents:
- PRX: 145,384 bytes; SHA-256 1f627b62bd48eb2f87fde95d34f7d91c07f3e5a008f5a4c0a81c472021d284c9
- fake-signed SPRX: 147,304 bytes; SHA-256 4de0ea6f49629818e4fd69d4f3e44e9357fff4f7481ad87ba07888e9542b574a
- ELF: 111,416 bytes; SHA-256 42befdeb3e5f4901fe7089ad5288819c451b02989a6358ea94ba601cfaaea246
- Arabic instructions and the Campaign/Multiplayer mode files.

This is still a read-only diagnostic PRX, not a Campaign co-op mod. It has passed CI validation but has not been loaded on a PS3.


## 9. Probe mode-label provenance

The Campaign/Multiplayer comparator now parses MODE_SOURCE and MODE_LABEL_IS_OPERATOR_SELECTED from each log. It reports both sources and marks the labels as operator-selected, not auto-detected. The combined result says both labels are operator-selected only when both logs carry the marker. A new test covers the case where only one log has provenance.

The final comparator/instructions commit passed all five CI jobs and 127/127 host tests (run 45023444205). The final documentation/instructions commit also passed Actions (run 45023497892).
