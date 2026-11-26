# Phase 115 — activator-aware owned-trigger adapter

## Objective

Move the compile-only owned-trigger pattern into the actual Campaign local-session bridge, so mission coordinators can obtain the real player entity emitted by an IW6 trigger event and acquire an event lock for that exact native local player.

## Bridge API

### `ghc_campaign_wait_for_owned_trigger(trigger_ent, event_id)`

1. Rejects a missing trigger, a session that is not ready, or an event ID outside the exact 0–15 range.
2. Validates the current native primary/secondary player pair before waiting.
3. Waits for `trigger_ent waittill("trigger", activator)`; the activator is taken from the event argument, not inferred from `level.player`, `self`, or a map struct.
4. Rejects an undefined activator.
5. Revalidates the native pair after the wait, because the session can change while the GSC thread is suspended.
6. Requires the activator to map to one of the two registered native Campaign players.
7. Acquires the shared event lock for that exact activator and returns the activator only after acquisition succeeds.

### Cleanup API

- `ghc_campaign_release_owned_trigger(player, event_id, completed)` delegates to the existing owner-only release API.
- `ghc_campaign_abort_owned_trigger(player)` releases the player's active event lock without marking the event complete.

The adapter does not create a player, clone an actor, assign a controller, or change engine client state.

## Verification

GitHub Actions run [#1642](https://github.com/Vikik0/PS3-SCALE/actions/runs/45863929336): **7/7 jobs successful**.

- Python suite: **606/606 tests**.
- Session bridge GSC compile and decompile: PASS.
- Session bridge source SHA-256: `33ef12fb9f8929cb1f70149ed40f7d5b7f5eb2edd636c21fe77bf7e5c83ab9b4`.
- Session bridge compiled GSCBIN SHA-256: `34e462ba5d70fcc5b53b7238d45c5841195644f324a73518bb2244ba68734024`; size **5,751 bytes**.
- Explicit-player helper fixtures: compile/decompile/recompile byte-for-byte PASS, including the trigger-activator identity guard.
- PS3 CEX PRX bootstrap, read-only probe builds, protocol checks and capture metadata intake: PASS.
- Capture kit: **57/57 files verified**, **540,916 bytes**, SHA-256 `a33fdcf4db0f0c6145f1e8c003010ad356029fec34dd1b415d3d7f0fe9853cde`; artifact ID `15139148167`.

## Mission integration gate

The bridge API is implemented and compiled, but no mission call site is migrated. Static review found 15 generic `waittill_trigger` call sites across the three supplied mission sources: 3 in Black Ice, 2 in Carrier and 10 in Cornered. Their caller functions do not yet provide a validated explicit player context. The four shared mission interactions from the earlier activator-lineage audit remain blocked until each interaction passes the real activator through its complete player-specific handler chain while keeping shared flags, animation timelines and mission coordinators single-owner.

## Runtime boundary

CI and GSC compiler results do not prove that a second native Campaign Local Client exists. No physical PS3 Campaign run, second-player camera/HUD, independent input, respawn lifecycle or shared-mission interaction has been demonstrated. Player 2 remains **NOT_PROVEN**; original shared interactions remain **0/4 ready**. No retail EBOOT, SELF or FastFile was modified or injected.
