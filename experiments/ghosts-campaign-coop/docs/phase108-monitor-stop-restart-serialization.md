# Phase 108 — serialize Campaign session-monitor shutdown and restart

## Race found

The previous idempotency guard treated an active watcher with a pending stop as safe to reuse by clearing the stop flag. That was not atomic with the watcher thread: the watcher could already have evaluated its loop condition and entered its shutdown path. Clearing the flag at that point did not prevent it from invalidating the session and exiting, while the caller returned as if the watcher were still active. This could leave a ready session without its only lifecycle monitor.

## Change

- `main()` now returns immediately only when the active watcher is not stopping.
- When a stop is pending, `main()` waits up to 40 × 50 ms (2 seconds) for the existing watcher to finish. It does not clear the stop flag or touch Campaign aliases while the old watcher is active.
- If the watcher remains active after the bounded wait, `main()` returns 0 and fails closed rather than creating a second owner or clearing live aliases.
- `ghc_campaign_start_session_monitor()` refuses to start a replacement watcher while the previous watcher is stopping.
- The watcher performs its existing fail-closed invalidation before clearing its active flag: release active drill/event locks, preserve completed-event history, restore the original player array and clear stale secondary aliases.
- The C host model now returns 0 for a start request while a stop is pending; after the stop tick completes, a fresh start is allowed and the native pair must bind again.

## Regression coverage

- Python source-contract tests verify the bounded wait occurs before alias cleanup and that the start helper refuses a second owner.
- C model test verifies start cannot cancel an in-flight stop, the stop releases active locks, completed-event history survives, and a new monitor can start after the old owner exits.
- Existing explicit-stop invalidation and automatic rebind tests remain enabled.

## Verification

- CI [#1614](https://github.com/Vikik0/PS3-SCALE/actions/runs/45784597522): **7/7 jobs successful**.
- Python: **577/577**.
- Campaign session-monitor C model: **16/16**.
- Event Lock C model: **21/21**.
- GSC wrapper parser: **10/10**.
- GSC wrapper loader: **12/12**.
- Host runtime simulator: **10,030 checks, 0 failures**.
- PS3 PRX bootstrap, GSC compile/decompile, capture metadata intake and capture-kit packaging all passed.
- Capture kit: **57/57 files**, **538,149 bytes**, SHA-256 `67244545d292af929b87b5dc841f3e605e1f95ed0220e6622e12e7a7bfa470e4`; artifact ID `15099792589`.

## Binary identity

- GSC source SHA-256: `410108baad1c665819594d438210acb9ab7ce6d8c4207740674e412f38513f20`.
- Compiled GSCBIN SHA-256: `6711fe3f34d1d98bc1e9f7653e74bd7b8a69dfb8c5ced15fa0fd141a1ae0f90e`.
- Compiled size: **4,124 bytes**.
- GSC Tool 1.6.2.364 compile → decompile → recompile was byte-identical.

## Boundary

No physical PS3 Campaign session or disconnect/reconnect cycle was available to this CI run. The changes establish lifecycle consistency in source, host model and compile checks; they do not prove that a second native Campaign Local Client exists or that the original four shared interactions work. The Player 2 gate remains `NOT_PROVEN` and the four interactions remain **0/4 ready**.
