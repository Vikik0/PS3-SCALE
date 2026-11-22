# Phase 12 — Clockwork drill ownership and local-session bridge hardening

## Goal

Continue the local-first design: reuse the two native split-screen players that Ghosts Multiplayer creates, then make Campaign interactions identify which of those two players is acting. Do not create a fake actor or a second input loop.

## Session startup hardening

Updated client/ps3/gsc/ghc_campaign_local_session.gsc:

- Startup clears stale Campaign aliases and retries native identity binding for up to 200 × 0.05 seconds.
- Each retry refreshes getentarray("player", "classname"), checks issplitscreen(), and attempts the full primary/secondary identity bind.
- It does not return early merely because two player entities exist; it waits for issplitscreenplayer() and issplitscreenplayerprimary() to report a valid pair.
- It binds level.player to the native primary and level.playengineeffects to the native secondary only after exact-pair validation.
- A rebind releases any active drill ownership for slots 0 and 1 before clearing the previous player references. Completed drill state remains shared and is not reset by a rebind.

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

GitHub Actions run 45031356086 passed all five jobs on commit 94ef85ef349fcaa41b2fbb8c4c4105b1964c06a4:

- Python: 158 tests passed.
- Protocol/runtime C tests: passed, including Campaign interaction ownership.
- Host runtime simulator: passed its 10,025 checks.
- GSC Tool 1.6.2.364: Campaign registry shim and local-session bridge compiled and round-trip decompiled.
- PS3DK: PRX bootstrap, Pad Probe and PRX importer build/validation passed.
- Cloudflare Worker: lobby tests and dry-run bundle passed.

The local-session GSCBIN is 1,798 bytes with SHA-256 dc846bbe1e3256f1fb8aae20913722335afe17105b61607322e890d892751a1f. Its source SHA-256 is 560417dcadd38187537c117dadff8f674021c664d9f94b79cd485096cb506a8f. The verified CI artifact is ghosts-campaign-gsc-session-bridge (artifact ID 14757777369).

## What remains unimplemented

The adapter is compile-tested source, not a playable feature:

1. The retail maps__clockwork_interior.gsc::handle_drill_spot and handle_drilling functions are not patched to pass the actual activating player into these helpers.
2. Weapon checks, ADS/fire input, stance, animation links, action slots and control locks still need to be routed to that same player in the original functions.
3. The shared drill-complete flag must be connected to the original objective trigger exactly once.
4. The bridge is not injected or automatically loaded by the retail Campaign VM.
5. Native Campaign local-client creation, controller 1 routing, independent camera/viewport/HUD, player state and respawn remain unproven on the target PS3.

GSC compilation and host tests prove source/contract integrity only. They do not prove runtime loading or in-game co-op.
