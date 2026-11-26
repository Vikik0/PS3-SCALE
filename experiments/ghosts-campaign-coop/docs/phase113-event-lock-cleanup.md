# Phase 113 — event-lock cleanup and stale-session query hardening

## Finding

Phase 112 validated the event-lock arrays, but cleanup and ownership-query paths still needed stronger lifecycle semantics. In particular, cleanup could report success even if invalid storage prevented either slot from being aborted. Release also needed to prioritize native-pair freshness so a stale session would still enter centralized invalidation even when the event-lock arrays were corrupt.

## Changes

- `ghc_campaign_event_lock_clear_active()` now checks the full state invariant before cleanup, aborts both slots, then checks the invariant again before reporting success. A slot with no active event remains a harmless no-op.
- `ghc_campaign_event_lock_release()` validates the initialized flag and session readiness, checks the native pair, and only then validates the event-lock arrays. A stale pair therefore invokes centralized invalidation rather than being masked by a lock-state failure.
- `ghc_campaign_event_lock_is_owner()` now returns true only for a ready session with a current native pair, valid event-lock state, a valid local player slot, and matching owner/active records. The query does not mutate or invalidate the session.
- Added three source-contract regression tests for cleanup, release ordering and ownership-query freshness.

## Verification

CI run [#1631](https://github.com/Vikik0/PS3-SCALE/actions/runs/45804585216): **7/7 jobs successful**.

- Python: **596/596**.
- Campaign session-monitor C model: **16/16**.
- Campaign Event Lock C model: **21/21**.
- GSC wrapper parser: **10/10**.
- GSC wrapper loader: **12/12**.
- Host runtime simulator: **10,030 checks, 0 failures**.
- GSC Tool 1.6.2.364 compile/decompile and manifest pin: PASS.
- PS3 CEX PRX bootstrap, read-only probes, import smoke-link and capture metadata intake: PASS.
- Capture kit: **57/57 files**, **540,322 bytes**, SHA-256 `fdd29448bc9a368cd6357cd345e9c7138e21c57635aaf776dcd36c0693a79517`; artifact ID `15109335074`.

## Binary identity

- GSC source SHA-256: `3f42c0c5f992266288eee059c238c031f617e236e0207432c72467fddc40db56`.
- Compiled GSCBIN SHA-256: `a57022f00419a91a153518058d2ecae0ee1121b601b4dff849bd0a1996efd9c3`.
- Compiled size: **5,545 bytes**.

## Boundary

This phase hardens source and host validation only. It does not authenticate runtime observations or create a native second Campaign Local Client. Physical PS3 Campaign capture, independent Player 2 camera/HUD and the four original shared mission interactions remain unproven. Player 2 remains `NOT_PROVEN`; shared interactions remain **0/4 ready**. No retail EBOOT or FastFile was modified.
