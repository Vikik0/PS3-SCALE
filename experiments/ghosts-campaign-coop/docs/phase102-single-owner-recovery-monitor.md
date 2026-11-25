# Phase 102 — prevent competing main/recovery monitor loops

## Change

The GSC `main()` entrypoint now checks whether the guarded session recovery monitor is already active and not stopping. If so, it returns without clearing aliases or starting another bounded bind loop.

This prevents a repeated `main()` call from racing the long-lived recovery watcher while it is validating or rebinding native player entities. The watcher remains the single owner of session recovery.

## Verification

- Python suite: **539/539 passed**.
- Event Lock C model: **21/21 passed**.
- GSC wrapper parser: **10/10 passed**.
- Mocked GSC loader: **12/12 passed**.
- GSC Tool compile/decompile, PS3 CEX PRX, both read-only controller probes and PRX importer link validation passed.
- CI run [#1586](https://github.com/Vikik0/PS3-SCALE/actions/runs/45722748409): **7/7 jobs successful**.
- Bridge source SHA-256: `4935a5e09c166246364cd36e7108d325ded57c41aa6f1668ff797bcae34b9ce2`.
- GSCBIN SHA-256: `25a22968eefc3e6b4dba6a9b5c8aa435f42cf2423577dd935e81e5a702dc2e6f`; size **4,010 bytes**.
- Capture kit: **57/57 files**, **537,728 bytes**, SHA-256 `d8c1cf5831beae8532976732c09c1620d0ab30680402d77b0a09d8421b838f3f`; artifact ID `15073380575`.

## Boundary

This prevents concurrent source-level recovery loops. It does not prove native Local Client availability or real reconnect behavior on the target PS3.
