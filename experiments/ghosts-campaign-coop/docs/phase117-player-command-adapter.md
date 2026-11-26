# Phase 117 — player-command owned-event adapter

## Purpose

Some Campaign interactions are not activated through an entity `"trigger"` event. They register a command notification directly on a player entity using `notifyonplayercommand`, then wait for that player to emit the named event. Carrier's rope-cut interaction is one example: it registers `"cut"` for `+melee`, `+melee_breath` and `+melee_zoom`.

Phase 117 adds a matching player-command adapter to the Campaign local-session bridge.

## API

`ghc_campaign_wait_for_owned_player_command(player, command, event_id)`

- Requires an explicit player entity, command event name, ready Campaign session and valid event ID.
- Validates the native primary/secondary player pair and confirms the player is one of those two entities before waiting.
- Waits on `player waittill(command)`.
- After the wait, checks the player still exists, revalidates the native pair and confirms the same player still maps to a valid slot.
- Acquires the shared event lock for that player and returns the player only after acquisition succeeds.

The mission handler must register `notifyonplayercommand` on the player before calling the adapter. It must then call the existing `ghc_campaign_release_owned_trigger` on successful completion or `ghc_campaign_abort_owned_trigger` on cancellation/failure.

## Why this is separate from the entity-trigger adapter

- `ghc_campaign_wait_for_owned_trigger(trigger_ent, event_id)` receives an activator from `trigger_ent waittill("trigger", activator)`.
- `ghc_campaign_wait_for_owned_player_command(player, command, event_id)` receives the event from a specific player entity after a registered player-command notification.

Both APIs revalidate the native pair after their wait and lock the exact player. Neither assumes that `level.player` is the only player.

## Verification

GitHub Actions run [#1646](https://github.com/Vikik0/PS3-SCALE/actions/runs/45865170243): **7/7 jobs successful**.

- Python suite: **607/607 tests**.
- Campaign local-session bridge GSC compile/decompile: PASS.
- Session bridge source SHA-256: `b2377f0cd7b8d1455e323555da28954b0d53a990a418efe408864956bfb247f6`.
- Compiled GSCBIN SHA-256: `85f7ddd1d91a1fa8e3a1d06bee3104d80c72937473433465b65b27153b6e04ed`; size **5,903 bytes**.
- PS3 PRX bootstrap, read-only probes, protocol checks and capture metadata intake: PASS.
- Capture kit: **57/57 files verified**, **541,248 bytes**, SHA-256 `7e680b92e451895e75f4dbe7a5324f62cce032da26cf808eacbf2f3971e91d29`; artifact ID `15140641180`.

## Integration boundary

The adapter is compiled into the experimental bridge, but no original mission function has been changed to call it. Carrier's rope-cut handler still uses the original `level.player` path; the new adapter is only the safe building block for a future per-player refactor. The shared rope entity, animation, mission flags and cut sequence still require one coordinator.

No physical PS3 Campaign test is claimed. Player 2 remains **NOT_PROVEN**; shared interactions remain **0/4 ready**. No retail EBOOT, SELF or FastFile was modified or injected.
