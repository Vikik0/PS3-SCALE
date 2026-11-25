# Phase 100 — automatic native-pair rebind after disconnect

## Change

The guarded Campaign session watcher no longer exits after the eighth consecutive native-pair validation failure. It invalidates the session, releases active drill/event ownership, clears stale aliases, then remains alive and polls the engine's current player array every 250 ms.

When split-screen is active and at least two player entities are present, it calls the existing strict native-pair binder. That binder still requires exactly two native split-screen identities, one primary and one secondary. It does not create or clone a player.

A stop flag and `ghc_campaign_stop_session_monitor()` provide an explicit way to stop the watcher. The watcher clears its active/stop state when it exits.

## Verification

- Python suite: **537/537 passed**.
- Event Lock C model: **21/21 passed**.
- GSC Tool compile/decompile and PS3 CEX PRX/probe builds passed.
- CI run [#1581](https://github.com/Vikik0/PS3-SCALE/actions/runs/45721984971): **7/7 jobs successful**.
- Bridge source SHA-256: `39a3c89234401710f1a367618a9b6a7f74469de26e4a586542d5ba21213ee51e`.
- GSCBIN SHA-256: `53d9acfd9af3d1f46643677b3ae80558a2e1a937fa49dddac18d5a3a9e6d7d4e`; size **3,962 bytes**.
- Capture kit: **57/57 files**, **537,428 bytes**, SHA-256 `7d2163b3232a6c169a56bf3239de05e1dc80f830378e4a459b3507ffd821cd52`; artifact ID `15072377465`.

## Boundary

This is a compile/source-contract result. It does not prove that a second native Campaign Local Client exists on the target PS3 or that the watcher can observe a real reconnect.
