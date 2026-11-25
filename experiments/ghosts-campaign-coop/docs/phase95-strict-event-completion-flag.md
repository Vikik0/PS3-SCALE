# Phase 95 — strict Event Lock completion input

## Change

The host C model accepted only exact boolean completion values, but the GSC release helper previously treated any truthy value as completion. The GSC API now rejects an undefined value and every value other than numeric 0 or 1 before it mutates Event Lock state.

- `0`: release/cancel without marking the event complete.
- `1`: release and mark complete.
- Any other value: reject without changing ownership or completion state.

This aligns the GSC bridge with the host-side contract model.

## Verification

- Python suite: **531/531 passed**.
- Event Lock C model: **21/21 passed**.
- GSC Tool compile/decompile and PS3 CEX bootstrap passed.
- CI run [#1569](https://github.com/Vikik0/PS3-SCALE/actions/runs/45719357177): **7/7 jobs successful**.
- Bridge source SHA-256: `42bcda255880184cc48bd7fd303fe99de9c01f546872fa797dee410d5f273fb0`.
- GSCBIN SHA-256: `5770976b355a13fa5259b67fe0490f6d4a415708cb112115d82d7c236d516d34`; size **3,828 bytes**.
- Capture kit: **57/57 files**, **535,838 bytes**, SHA-256 `80dc748e8e1b2cf3aabe2b126646ac3d1f32c1075a87e15e029cc6f2168a9c2a`; artifact ID `15070344311`.

## Boundary

No original mission GSC was changed and no retail runtime result is claimed.
