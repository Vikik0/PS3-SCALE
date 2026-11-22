# Phase 11 — Reuse Ghosts native local Multiplayer session in Campaign

## Decision

The preferred route is to reuse the local split-screen player/session that Ghosts already creates in a supported offline Multiplayer mode. Campaign should consume those same engine-owned local players rather than inventing a second input system or spawning a visual clone.

The key distinction is between two layers:

1. **Native engine/session layer:** creates local-client slots, routes controller 0/1, owns each player's command stream and view/camera. This is the unresolved PPU/runtime gate.
2. **GSC gameplay layer:** registers already-created player entities, identifies primary/secondary split-screen players, and applies Campaign gameplay rules. Campaign already has much of this layer.

Copying Multiplayer's match clientid allocator is not the solution. That ID is match-data bookkeeping and is not the local-client slot or controller-port number.

## Evidence reused

- Campaign's shared init_level_flags() already builds level.playerpiggyback from getentarray("player", "classname"), sets level.player, and conditionally stores a second player in level.playengineeffects.
- Campaign shared scripts already use issplitscreen() and contain second-player-aware helpers.
- Multiplayer GSC uses issplitscreenplayer() and issplitscreenplayerprimary() to distinguish each native local player.
- Multiplayer's connect/begin lifecycle registers an engine-created player entity after the engine's native connection path. It does not itself create the native local client.
- Clockwork's original setup_combat() has a known starting loadout for Logan: the arctic FED view hands, GM6, CZ805, frag and flash.

These findings justify reusing the native split-screen identities and Campaign's existing player aliases. They do not prove that Campaign can currently start with two native local players.

## New implementation

Added client/ps3/gsc/ghc_campaign_local_session.gsc.

### 1. Bounded session discovery

The bridge refreshes the Campaign player list from getentarray("player", "classname") for up to 200 × 0.05 seconds. It does not wait on an undocumented notification and does not leave an unbounded script thread waiting forever.

It proceeds only when the engine reports split-screen active and at least two player entities are present.

### 2. Native identity validation

The bridge ignores entities that do not satisfy issplitscreenplayer(). It assigns slot 0 only to the entity for which issplitscreenplayerprimary() is true, and slot 1 to the other native split-screen player.

It fails closed for:
- missing player array;
- split-screen inactive;
- duplicate primary or secondary identity;
- player count other than exactly two;
- missing primary or secondary identity.

### 3. Campaign alias binding

Only after the exact native pair passes validation, the bridge binds:

- level.player → native primary player (Logan / Player 1);
- level.playengineeffects → native secondary player (guest soldier / Player 2).

It also assigns unique_id values player0 and player1 according to native identity. It does not assume that getentarray() returns the players in primary-first order.

### 4. First mission vertical slice: Clockwork starting loadout

The bridge exposes ghc_campaign_apply_clockwork_player_two_loadout(). It applies the same starting kit as the original Clockwork setup_combat() to the validated secondary player only:

- viewhands_fed_army_arctic;
- gm6+scopegm6_sp+silencer03_sp;
- cz805bren+reflex_sp+silencer_sp;
- frag and flash.

Player 1's original setup function is deliberately left untouched. The helper is guarded by the session-ready flag, rechecks native secondary identity, and is idempotent within the mission.

The intended future call site is immediately after the original setup_combat() has completed for Player 1. The bridge is not yet injected or automatically called by the retail Campaign script.

## Safety and scope

The bridge:
- does not create or clone a player;
- does not call PlayerCMD_Spawn or PlayerCMD_ClonePlayer;
- does not set coop=1;
- does not poll cellPadGetData or compete with the game's input loop;
- does not write raw game memory or patch the EBOOT;
- does not change Hesh or any story NPC;
- does not claim to create controller 1's native client slot.

The native engine must still provide two real split-screen players with separate controller routing, client/player state and view contexts. If that native pair is absent, the bridge leaves Campaign aliases unchanged and reports a failure status.

## Verification added

- Eight Python source-contract tests cover native-player reuse, bounded discovery, primary/secondary identity, fail-closed alias binding, slot access, the Player-2-only Clockwork loadout, idempotence and the absence of fake spawn/input-poll logic.
- The pinned GSC Tool CI job compiles and round-trip decompiles the new IW6/PS3 GSC bridge.
- The older read-only registry diagnostic was also changed from an undocumented level.players initialized wait to a bounded wait, avoiding a possible indefinite hang.

GSC compilation proves syntax and bytecode round-trip only. It does not prove that the bridge can be loaded into the retail Campaign VM.

## Remaining native gate

The next implementation step is to identify the native PS3 TU 1.16 local-client/session initialization used by offline Multiplayer and establish whether Campaign can enter or retain that path. This requires a verified decrypted/runtime PPU image or equivalent controlled runtime observations. Do not copy MW2 offsets, public PS4 code, or unverified Multiplayer addresses into Campaign.

Once that native path is proven:
1. connect it to Campaign startup;
2. run the bridge after the engine has created both local players;
3. call the Player 2 loadout helper after Clockwork's original Player 1 loadout;
4. adapt the first player-specific interaction (the drill) without duplicating shared mission flags;
5. add independent camera/HUD, death/respawn, checkpoint and transition tests.

**Phase 11 status:** the Campaign-side GSC reuse bridge and first Player 2 loadout adapter are implemented as compile-tested source. Native Player 2 creation, runtime loading, camera/HUD and playable Campaign co-op remain unproven.

## Clockwork drill ownership contract

Added a portable contract in client/ps3/campaign_interaction.h and campaign_interaction.c, with host tests in tests/campaign_interaction_test.c.

It models the shared-interaction rules needed by the first Clockwork drill adaptation:
- one active interaction per local player;
- different players may operate different drill spots at the same time;
- one drill spot has only one owner;
- only the owner may complete or cancel that spot;
- completion is shared and cannot be committed twice;
- cancel, death or disconnect releases the owner's lock without marking the objective complete.

This prevents Player 2 from completing, cancelling or stealing Player 1's drill action. The portable state machine is testable now, but the retail Clockwork GSC has not yet been rewritten to call it.

## Native function discovery tooling

Added a Ghidra exporter and Python comparator for the exact next research step: finding the existing Multiplayer local-client/player lifecycle and comparing it with Campaign.

- tools/ghidra/ExportGhostsPpuFunctionInventory.java exports normalized function fingerprints and direct call targets from one decrypted PPU ELF.
- tools/compare_ppu_function_inventories.py compares Campaign and Multiplayer inventories, then ranks Multiplayer-only function names related to local clients, controllers, players, views, cameras, HUD and input.
- tests/test_compare_ppu_function_inventories.py covers shared fingerprints, Multiplayer-only leads, duplicate signatures and malformed inputs.

The Ghidra exporter is read-only and requires manual execution on both exact TU 1.16 images. The Python comparator is covered by the normal host test suite. Neither tool proposes a hook address.

## Clockwork GSC drill ownership helpers

The two-spot Clockwork ownership state now lives directly inside ghc_campaign_local_session.gsc and is initialized once after the validated native player pair is bound.

The bridge exposes:
- ghc_campaign_drill_begin(player, spot): claim spot 0 or 1 for the validated player, only if that player is not already using another spot and the spot is neither owned nor complete.
- ghc_campaign_drill_finish(player, spot, completed): owner-only finish; marks shared completion only when completed is true, then releases the spot.
- ghc_campaign_drill_complete(player, spot) and ghc_campaign_drill_cancel(player, spot): explicit finish/cancel wrappers.
- ghc_campaign_drill_abort_player(player): releases that player's active spot on death/disconnect/cutscene exit without marking it complete.
- owner/completion accessors for mission-side checks.

This is more direct than maintaining a second generic GSC owner manager. The portable C interaction state machine remains the host-tested model for race and ownership behavior.

The helper is not yet wired into Clockwork's original handle_drill_spot() because that retail GSC source is not committed in the repository. Do not mark the drill as co-op integrated until the original function passes the actual player entity through input, weapon, animation, control-lock and cleanup paths.
