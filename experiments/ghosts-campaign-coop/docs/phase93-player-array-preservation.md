# Phase 93 — preserve the original Campaign player array on invalidation

## Finding

The session bridge's invalidation path cleared `level.playerpiggyback` to `undefined`. That variable is an original Campaign player-entity array used by unrelated mission scripts, not a private secondary-player alias. Clearing it after a secondary-client disconnect could leave original mission code without the array it expects.

## Change

`ghc_campaign_clear_session_aliases()` now rebuilds `level.playerpiggyback` from the current `getentarray("player", "classname")` result, uses an empty array if the engine returns no defined array, preserves the previous primary if that entity is still present, and otherwise falls back to the first defined live player. It clears only the stale secondary alias and bridge-specific player handles.

The array is restored before the bridge clears its private player handles. This keeps unrelated Campaign scripts able to enumerate current player entities after session invalidation.

## Verification

- Python suite: **529/529 tests passed**.
- Event-lock C model: **21/21 cases passed**.
- GSC Tool 1.6.2 compile/decompile passed.
- Source SHA-256: `91beae5278cddaa23521aabdfec134e9323f8b28a7f610b470709a4236da4e8b`.
- Compiled GSCBIN SHA-256: `e88a92b314928c36ee55fbbcba1593dd3e0a6ed7d719bf75de4743e750a72ad9`; size **3,800 bytes**.
- CI run **#1564**: **7/7 jobs successful**, including PS3 CEX bootstrap, both read-only controller probes, GSC fixture round-trips, and capture-kit packaging.
- Capture kit: **57/57 files verified**; inner ZIP size **535,522 bytes**; SHA-256 `a62b25f7b2c201d6f2714872d6c916df1650e68fcf83f20537dc6d16d96f59f1`; artifact ID `15070258524`.

## Limits

This fixes invalidation behavior at source level. It does not prove the retail game invokes the bridge or that a second native Local Client exists in Campaign. No original mission handler was modified, no FastFile was rebuilt/injected, and no physical PS3 runtime result is claimed. The four shared mission interactions remain **0/4 ready for wiring** until the exact TU 1.16 native player pair and trigger activator semantics are verified.
