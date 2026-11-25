# Phase 92 — player-aware trigger adapter fixture

## Purpose

Phase 90 showed that the four shared mission interactions need a separate player-aware trigger path. The existing generic helpers are reused by unrelated mission call sites, so they must not be rewritten globally. This phase adds a compile-only reference fixture for a dedicated adapter.

## Adapter flow

`ghc_campaign_wait_for_owned_trigger(trigger_ent, event_id)`:

1. Rejects a missing trigger entity, a session that is not ready, or an Event ID outside 0–15.
2. Validates the exact native primary/secondary player pair before waiting.
3. Waits on the supplied trigger entity and captures the event's `activator` argument.
4. Revalidates the native pair after the wait, because the pair may have changed while the function was suspended.
5. Confirms that the captured activator is one of the two registered native players.
6. Acquires the requested Event Lock for that exact activator.
7. Returns the activator only after the lock succeeds; otherwise it returns `undefined`.

The fixture also provides explicit-player release and abort wrappers. It never substitutes `level.player`, `self`, or a map struct for the activating player.

## Compiler-only signatures

The fixture includes local stub definitions for the bridge functions it calls. GSC Tool needs these signatures to resolve the isolated fixture's function calls. They are compile-only stubs; the real implementations remain in `ghc_campaign_local_session.gsc`. The fixture and stubs are not copied into the PS3 payload and must never be loaded beside the real bridge.

## Verification

- Six Python source-contract tests verify the ordering of the pre-wait checks, post-wait native-pair validation, activator ownership check, lock acquisition, and explicit-player release/abort.
- GSC Tool 1.6.2 compiles the fixture, decompiles it, and recompiles it byte-for-byte.
- The normal host suite includes these tests.
- CI run **#1561**: **7/7 jobs successful**, **528 Python tests passed**, **21/21 event-lock C model cases passed**. The PS3 CEX bootstrap, read-only controller probes, and capture-kit validation also passed.
- Capture kit: **57/57 files verified**; inner ZIP size **535,224 bytes**; SHA-256 `89acce060f71686385b160709cda6ac47d5e44dac04999ab205c79645a15a196`; artifact ID `15070815301`.

## Limits

A GSC compile/decompile round-trip proves syntax and bytecode stability only. It does not prove that the retail IW6/PS3 trigger event supplies the expected activator at runtime, that the bridge is loaded by Campaign, or that a second native Local Client exists.

This fixture is not an implementation of Black Ice, Carrier, or Cornered. No original mission GSC was modified, no FastFile was rebuilt/injected, and no physical PS3 result is claimed. The original mission readiness remains **0/4**.

## Next gate

After the exact TU 1.16 native second-player path is verified, use a controlled PS3 test to confirm the trigger event's activator semantics. Then migrate only the first intended mission call site to the dedicated adapter, keeping the mission coordinator and shared state single-owner. Do not rewrite the generic trigger helpers globally.
