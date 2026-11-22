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

The same init_level_flags() function in the supplied patch_common/1002.gsc overlay is byte-for-byte identical to the base common/1002.gsc function. The update overlay therefore does not remove this player-array setup.

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
