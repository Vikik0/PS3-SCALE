# Phase 101 — start the recovery watcher even when initial binding fails

## Change

The GSC `main()` entrypoint now starts the guarded recovery watcher on all terminal startup paths:
- no fresh player array after the bounded startup retry;
- a fresh player array exists, but the strict native-pair bind still fails;
- the native pair binds successfully (existing behavior).

This means the bridge can wait for a local split-screen pair that appears after the initial startup window. A later native pair can be bound without a second manual `main()` call. The watcher still binds only engine-created entities and retains the exact primary/secondary identity checks.

## Verification

- Python suite: **538/538 passed**.
- Event Lock C model: **21/21 passed**.
- Portable GSC wrapper parser: **10/10 passed**.
- Mocked GSC file loader: **12/12 passed**.
- GSC Tool compile/decompile, PS3 CEX PRX, controller probes and importer link validation passed.
- CI run [#1583](https://github.com/Vikik0/PS3-SCALE/actions/runs/45722241399): **7/7 jobs successful**.
- Bridge source SHA-256: `8acdcca9de55a7a962e9d631af96f581aa1d7531367ec8341795319756bbff10`.
- GSCBIN SHA-256: `9ad7958049df3ad173a86d5b1703405bca7ac5c8fa496b07bcdb3244325d8735`; size **3,972 bytes**.
- Capture kit: **57/57 files**, **537,563 bytes**, SHA-256 `287e179f42e0fc41368f08fe7cefab4ccfa4af6ea6f762b20ef36fd6957c6ba3`; artifact ID `15072508057`.

## Boundary

Automatic retry is implemented in the GSC bridge, but no physical PS3 Campaign runtime observation has yet verified the engine's player-entity and split-screen identity transitions.
