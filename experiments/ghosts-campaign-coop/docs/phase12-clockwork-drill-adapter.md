# Phase 12 — Clockwork drill ownership and local-session bridge hardening

## Goal

Continue the local-first design: reuse the two native split-screen players that Ghosts Multiplayer creates, then make Campaign interactions identify which of those two players is acting. Do not create a fake actor or a second input loop.

## Session startup hardening

Updated client/ps3/gsc/ghc_campaign_local_session.gsc:

- Startup reuses an already-ready session only if both stored entities are still native split-screen players, primary/secondary roles are unchanged, and the Campaign aliases still point to those same entities.
- If any identity check fails, it releases active drill ownership, clears stale Campaign aliases and retries native identity binding for up to 200 × 0.05 seconds.
- Each retry refreshes getentarray("player", "classname"), checks issplitscreen(), and attempts the full primary/secondary identity bind.
- It does not return early merely because two player entities exist; it waits for issplitscreenplayer() and issplitscreenplayerprimary() to report a valid pair.
- It binds level.player to the native primary and level.playengineeffects to the native secondary only after exact-pair validation.
- A rebind releases any active drill ownership for slots 0 and 1 before clearing the previous player references. Completed drill state remains shared and is not reset by a rebind.
- Every successful native-pair bind resets `ghc_campaign_clockwork_p2_loadout_applied` before marking the session ready. A replacement secondary entity therefore receives its own Clockwork loadout; the previous pair's idempotency flag cannot suppress loadout application after controller disconnect/rebind.

## Clockwork two-spot ownership

The same GSC bridge now initializes two drill spots once after the native pair is validated.

Available helpers:

| Helper | Purpose |
|---|---|
| ghc_campaign_drill_begin(player, spot) | Claim spot 0 or 1 for a validated local player. |
| ghc_campaign_drill_finish(player, spot, completed) | Owner-only finish; optionally commits the shared completion flag. |
| ghc_campaign_drill_complete(player, spot) | Complete the spot and release its owner lock. |
| ghc_campaign_drill_cancel(player, spot) | Release the owner lock without completing the spot. |
| ghc_campaign_drill_abort_player(player) | Resolve the player's slot and abort its active spot. |
| ghc_campaign_drill_abort_slot(slot) | Abort by native slot, including after the player entity is no longer available. |
| ghc_campaign_drill_owner_for_spot(spot) | Read the current owner slot. |
| ghc_campaign_drill_is_complete(spot) | Read the shared completion flag. |

Rules:

- Each local player can own at most one active drill spot.
- Player 1 and Player 2 may operate different spots concurrently.
- A spot cannot be claimed by both players.
- Only the owner may complete or cancel a spot.
- Completion is shared mission state and is set only once.
- Cancel, death, disconnect or session rebind releases ownership without falsely completing the objective.
- Claim and finish critical sections contain no wait/yield, so the script VM cannot interleave another GSC event between the owner check and assignment.

## Portable test model

The existing C state machine in client/ps3/campaign_interaction.c remains the host-test model for ownership, double claims, cancellation, completion and abort. It is not called from GSC and is not injected into the game.

## Build verification

GitHub Actions run 45032662329 passed all five jobs on commit 94ef85ef349fcaa41b2fbb8c4c4105b1964c06a4:

- Python: 164 tests passed.
- Protocol/runtime C tests: passed, including Campaign interaction ownership.
- Host runtime simulator: passed its 10,025 checks.
- GSC Tool 1.6.2.364: Campaign registry shim and local-session bridge compiled and round-trip decompiled.
- PS3DK: PRX bootstrap, Pad Probe and PRX importer build/validation passed.
- Cloudflare Worker: lobby tests and dry-run bundle passed.

After the Player 2 rebind-guard fix, CI rebuilt the local-session GSCBIN at 2,372 bytes with SHA-256 `545c064f21aad28c5d3d636adaf4d8b1009da852a446a394267e54570c1b44b2`. Its source SHA-256 is `ae46676fb265aa334759e7700a7f2a9b7c305e2c6868393afddbf94421b4c58f`. Both values are pinned in `client/ps3/gsc/ghc_campaign_local_session.manifest.json`. The CI artifact is `ghosts-campaign-gsc-session-bridge`.

## What remains unimplemented

The adapter is compile-tested source, not a playable feature:

1. The retail maps__clockwork_interior.gsc::handle_drill_spot and handle_drilling functions are not patched to pass the actual activating player into these helpers.
2. Weapon checks, ADS/fire input, stance, animation links, action slots and control locks still need to be routed to that same player in the original functions.
3. The shared drill-complete flag must be connected to the original objective trigger exactly once.
4. The bridge is not injected or automatically loaded by the retail Campaign VM.
5. Native Campaign local-client creation, controller 1 routing, independent camera/viewport/HUD, player state and respawn remain unproven on the target PS3.

GSC compilation and host tests prove source/contract integrity only. They do not prove runtime loading or in-game co-op.

## Live native-pair revalidation

The bridge now has a separate read-only check, ghc_campaign_native_pair_is_current(), which refreshes getentarray and confirms that:

- the native split-screen mode is still active;
- the stored primary and secondary handles are still native split-screen players;
- their primary/secondary roles have not changed;
- the live player entity array still contains those exact two handles and no third local split-screen player.

The bridge uses this check before reusing an already-ready session, applying Player 2's loadout, and beginning or finishing a drill interaction. If the pair is no longer valid, it marks the session not ready and releases both drill locks without marking either spot complete. This closes the stale-entity case where a controller disconnects or the engine replaces one local player while the Campaign script still holds an old entity reference.

If validation fails, ghc_campaign_invalidate_local_session() sets status 8, releases both drill locks, clears the stale secondary alias and player handles, and preserves level.player only when the stored primary is still a valid native primary player. A fresh main() call can then attempt a bounded rebind. Initial single-player Campaign startup does not clear the normal level.player alias because this cleanup path runs only after a previously ready local pair becomes invalid.

