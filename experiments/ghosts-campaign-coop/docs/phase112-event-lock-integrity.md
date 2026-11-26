# Phase 112 — validate shared Campaign event-lock state

## Finding

The shared Campaign event lock used numeric range checks for event IDs and player slots, and its operations trusted the initialized flag without validating the backing arrays. Fractional IDs could pass the old range checks and be used as array indices. Corrupted owner/active links or completion values could also be read as valid state. Reinitializing an already initialized but corrupted lock risked erasing completed-event history.

## Changes

- Added exact event-ID validation for `0..15` and exact slot validation for `0/1`.
- Added a state validator for owner, active and completion array existence and exact sizes `16/2/16`; valid owner/completion enums; owner-to-active and active-to-owner consistency; and the rule that completed events cannot remain owned.
- `ghc_campaign_event_lock_init()` validates existing initialized state and refuses to reset it when corrupt. Fresh initialization is also validated.
- Acquire, release, owner queries, abort and reset validate state and exact IDs before array reads or writes.
- Invalid owner and completion queries return distinct sentinels: `-2` for owner and `-1` for completion.
- Native Campaign pair binding checks the event-lock initializer result. Invalid storage causes alias cleanup, status `9`, and a failed bind instead of marking the session ready.
- Added six regression tests for exact IDs, array invariants, corruption handling, query sentinels, abort/reset and bind failure.

## Verification

CI run [#1628](https://github.com/Vikik0/PS3-SCALE/actions/runs/45803806486): **7/7 jobs successful**.

- Python: **593/593**.
- Campaign session-monitor C model: **16/16**.
- Campaign Event Lock C model: **21/21**.
- GSC wrapper parser: **10/10**.
- GSC wrapper loader: **12/12**.
- Host runtime simulator: **10,030 checks, 0 failures**.
- GSC Tool 1.6.2.364 compile/decompile and manifest pin: PASS.
- PS3 CEX PRX bootstrap, read-only probes, import smoke-link and capture metadata intake: PASS.
- Capture kit: **57/57 files**, **540,179 bytes**, SHA-256 `ca1d34d5201e42941d923de11c9e73c207e223be3551653353eae980043c0516`; artifact ID `15107823638`.

## Binary identity

- GSC source SHA-256: `fd179a44d0aba94e6c42b32d29755093f8a7e6b730f88f63c75ca599d6147c7a`.
- Compiled GSCBIN SHA-256: `e63db21a6436325023c7bc142719c07fc0f152224e142782f55a2d647b626d52`.
- Compiled size: **5,508 bytes**.

## Boundary

This phase validates the event-lock contract and compiled source; it does not authenticate runtime observations or create a second Campaign Local Client. Physical PS3 Campaign capture, independent Player 2 camera/HUD, and integration of the four original shared mission interactions remain open. Player 2 remains `NOT_PROVEN`; shared interactions remain **0/4 ready**. No retail EBOOT or FastFile was modified.
