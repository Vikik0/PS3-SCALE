# Phase 110 — guard Clockwork drill state setters

## Finding

The Phase 109 finish API rejected malformed completion values, but the lower-level state setters remained callable without checking whether the shared drill state had been initialized. They also accepted undefined or out-of-range slot/owner/active-spot values. A direct helper call could therefore mutate state outside the normal validated begin/finish path.

## Changes

- `ghc_campaign_drill_set_active_spot_for_player()` now requires initialized drill state, a defined player slot in `0..1`, and an active-spot value in `-1..1`.
- `ghc_campaign_drill_set_owner_for_spot()` applies the same initialization and range checks to the spot and owner slot.
- `ghc_campaign_drill_set_complete_for_spot()` requires initialized state, a valid spot ID, and an explicit numeric completion flag of exactly `0` or `1`.
- Every invalid call returns before the first state write.
- Normal ownership transitions, owner-only finish, cancellation and completed-history behavior are unchanged.

## Verification

CI [#1622](https://github.com/Vikik0/PS3-SCALE/actions/runs/45787583744): **7/7 jobs successful**.

- Python: **583/583**.
- Campaign session-monitor C model: **16/16**.
- Campaign Event Lock C model: **21/21**.
- GSC wrapper parser: **10/10**.
- GSC wrapper loader: **12/12**.
- Host runtime simulator: **10,030 checks, 0 failures**.
- GSC Tool 1.6.2.364 compile/decompile and manifest pin: PASS.
- PS3 PRX bootstrap/probes/import smoke-link and capture metadata intake: PASS.
- Capture kit: **57/57 files**, **538,971 bytes**, SHA-256 `0217d080d58da690e6ba8931e8ae74c32bdf0fbe21f3170be5e62e31abc89b96`; artifact ID `15100798110`.

## Binary identity

- GSC source SHA-256: `875b21dd7f47004257c9ef156eba44a27b448c4967122a9136b6d0a43fd003a8`.
- Compiled GSCBIN SHA-256: `3f588ac0004b47c3f2e4c0245e22454767501540b3f7b2f01d5bfc016fd53a35`.
- Compiled size: **4,378 bytes**.

## Boundary

This is API/state hardening validated by CI and the pinned compiler, not a physical PS3 test. The native second Campaign Local Client, independent Player 2 camera/HUD and four shared mission interactions remain unproven. Player 2 remains `NOT_PROVEN`; shared interactions remain **0/4 ready**. No retail EBOOT, FastFile or original mission source was modified.
