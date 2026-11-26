# Phase 111 — fail-closed Clockwork drill state reads and transactional updates

## Finding

Phase 110 guarded the three drill state setters, but the corresponding getters still exposed state before initialization and could return values from missing or corrupted backing fields. The accepted numeric ranges also allowed fractional values such as a 0.5 owner or active spot to be written even though the state machine only defines discrete player/spot IDs.

The begin, finish and abort paths also ignored setter return values. A future guard change or unexpected state invalidation could therefore leave a partially claimed or partially released drill interaction.

## Changes

- Owner and active-spot getters now require initialized state, a defined exact ID, and a backing field in the allowed enum. Invalid reads return `-2`, which is outside the valid owner/active values `-1, 0, 1`.
- Completion reads now require initialized state, a defined exact spot ID, and a stored boolean of exactly `0` or `1`. Invalid reads return `-1`, outside the valid completion values.
- Owner and active setters accept only exact slot/spot enums: slot/spot ID `0` or `1`, and owner/active value `-1`, `0`, or `1`.
- Completion setters accept only exact spot IDs and explicit numeric booleans.
- Begin rejects invalid player slots and spot IDs, and only treats Stage 1 as complete when the getter returns exactly `1`. Unknown/corrupt completion state blocks both stages.
- Begin checks both setter results and clears the owner if the active-player write fails.
- Finish checks completion and release writes. If a later release step fails, it restores the prior owner and clears the just-written completion flag.
- Abort validates exact slot and active-spot values and restores the owner if clearing the active slot fails.
- Added four regression tests for getter guards, discrete enums and rollback ordering.

## Verification

CI run [#1625](https://github.com/Vikik0/PS3-SCALE/actions/runs/45802942130): **7/7 jobs successful**.

- Python: **587/587**.
- Campaign session-monitor C model: **16/16**.
- Campaign Event Lock C model: **21/21**.
- GSC wrapper parser: **10/10**.
- GSC wrapper loader: **12/12**.
- Host runtime simulator: **10,030 checks, 0 failures**.
- GSC Tool 1.6.2.364 compile/decompile and manifest pin: PASS.
- PS3 CEX PRX bootstrap, read-only Pad Probe, DualShock input probe, import smoke-link and capture metadata intake: PASS.
- Capture kit: **57/57 files**, **539,423 bytes**, SHA-256 `6c67a3895a27caebacd411a79568212e1aa8d24771652f2afe31d8f8bb1306e1`; artifact ID `15108372304`.

## Binary identity

- GSC source SHA-256: `8ccd5ea42cd4c0e734bda6adae8f85a8498c2ba8e349cabfd8bfee6e1cfa2d9d`.
- Compiled GSCBIN SHA-256: `d0743de26d5352ba1da9466a639d1506f760265152b7b3d96651ec4785c970f6`.
- Compiled size: **4,851 bytes**.

## Boundary

This phase hardens the GSC API and its source-level regression coverage. It does not create a second Campaign Local Client and is not a physical PS3 runtime test. Independent Player 2 camera/HUD, original mission trigger delivery, and the four shared mission interactions remain unproven. Player 2 remains `NOT_PROVEN`; shared interactions remain **0/4 ready**. No retail EBOOT or FastFile was modified.
