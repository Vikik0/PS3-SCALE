# Phase 91 — Campaign event-lock contract model

## Purpose

The Campaign Local Session bridge already contains a 16-ID GSC event-lock manager. This phase adds an independent host-side model so its ownership and cleanup rules can be exercised with deterministic tests without pretending that the GSC runs on a retail PS3.

## State contract

- 16 event IDs: 0–15.
- Two validated local-player slots: 0 and 1.
- Each event has at most one owner.
- Each player may own at most one active event.
- Different players may own different event IDs at the same time.
- A completed event stays unavailable until the explicit reset helper is called.
- Release is owner-only. A successful release may mark the event complete; cancellation releases without completion.
- Player abort and session cleanup release active ownership without marking the event complete.
- Session invalidation clears active ownership but preserves completed-event history.
- Acquire and release fail closed when the session is not ready or the native pair is stale.

The model requires exact 0/1 boolean inputs and provides an invariant checker for owner/active-slot consistency. Its initialization is idempotent and preserves completed history, matching the GSC bridge's rebind behavior.

## Files added

- `tests/campaign_event_lock_model.h`
- `tests/campaign_event_lock_model.c`
- `tests/campaign_event_lock_model_test.c`
- `tests/test_campaign_event_lock_model.py`

The C implementation is explicitly a **host-only test model**. It is not linked into the PS3 PRX and does not add a runtime hook or alter game memory.

## Verification

Run the C contract tests:

```sh
cc -std=c11 -O2 -Wall -Wextra -Werror tests/campaign_event_lock_model.c tests/campaign_event_lock_model_test.c -o /tmp/campaign-event-lock-model-test
/tmp/campaign-event-lock-model-test
```

The normal Python suite also checks the source-level GSC contract and the stable Event IDs from Phase 88. The Windows host runner now builds and runs the C model as part of its standard test sequence.

## Phase 91 result

- C event-lock model: **21/21 cases passed**.
- Python suite: **522/522 tests passed**.
- GitHub Actions run **#1558**: **7/7 jobs successful**.
- GSC Tool compile/decompile validation passed.
- PS3 CEX bootstrap and both read-only controller probes built and validated.
- Reproducible TU 1.16 development capture kit: **57/57 files verified**; inner ZIP size **535,221 bytes**; SHA-256 `76ca5d4daa7eae63ea54ca08ef6543a451909d945362d7b4e4558f8052199499`.
- Uploaded capture-kit artifact ID: `15069646947`.

## Limits and next gate

This phase validates the event-lock state contract only. It does **not** make the four Black Ice/Carrier/Cornered handlers player-aware, create a native second Local Client, or prove that Campaign accepts the compiled bridge.

The Phase 89/90 result remains unchanged: **0/4** shared mission interactions are safe to wire until the original trigger path passes the actual activator into a player-specific handler. No original mission GSC was changed, no FastFile was rebuilt or injected, and no physical PS3 runtime result is claimed.

Next native step remains verification of the exact TU 1.16 Campaign/Multiplayer local-client lifecycle from user-owned decrypted PPU images or controlled PS3 runtime observations. Only after that gate passes should the player-aware trigger adapter be integrated into the original mission call sites.
