# Phase 109 — strict Clockwork drill API input validation

## Finding

The shared Clockwork drill helper accepted a generic truthy/falsy completion value. That meant an undefined or non-boolean value could reach the finish path and be interpreted as a successful completion, even though the event-lock API already required exact numeric booleans. The spot argument also relied only on numeric range comparisons, without explicitly rejecting an undefined value.

## Changes

- `ghc_campaign_drill_begin()` now rejects an undefined spot ID before any ownership state changes.
- `ghc_campaign_drill_finish()` accepts only an explicitly defined numeric `0` or `1` completion flag. Other values return `0` before lock ownership or completion state is changed.
- The finish path also rejects an undefined or out-of-range spot ID.
- `ghc_campaign_drill_set_complete_for_spot()` validates the slot and completion value independently, so direct helper calls cannot write malformed completion state.
- Existing owner-only completion, Stage 1 → Stage 2 sequencing, and abort-without-completion behavior remain unchanged.

## Verification

CI [#1619](https://github.com/Vikik0/PS3-SCALE/actions/runs/45786580232): **7/7 jobs successful**.

- Python: **581/581**.
- Campaign session-monitor C model: **16/16**.
- Campaign Event Lock C model: **21/21**.
- GSC wrapper parser: **10/10**.
- GSC wrapper loader: **12/12**.
- Host runtime simulator: **10,030 checks, 0 failures**.
- GSC Tool 1.6.2.364 compile/decompile and manifest pin check: PASS.
- PS3 CEX PRX bootstrap, pad probes, importer link and capture-metadata intake: PASS.
- Capture kit: **57/57 files**, **538,860 bytes**, SHA-256 `d0ae425baa7f139b701b29e20189eddd29c7fb67b579f364dde45b66658a11b1`; artifact ID `15100672886`.

## Binary identity

- GSC source SHA-256: `8624e61d8f7d961c39b6f4965b4f4a43ff2a7db2cc052775852f9a8d24d13dd3`.
- Compiled GSCBIN SHA-256: `67da3cc6169a4a02071f3ddfe43df2750136e0248b18444897029145a2099182`.
- Compiled size: **4,217 bytes**.

## Boundary

This is source-level API hardening validated by host tests and the pinned GSC compiler. It is not a physical PS3 runtime test. It does not prove a second Campaign Local Client, independent Player 2 camera/HUD, or any of the four shared mission interactions. Player 2 remains `NOT_PROVEN`; shared interactions remain **0/4 ready**. No retail EBOOT, FastFile or original mission script was modified.
