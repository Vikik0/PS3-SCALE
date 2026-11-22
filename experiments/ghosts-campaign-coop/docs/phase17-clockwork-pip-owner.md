# Phase 17 — Clockwork drill PIP ownership

## Goal

The retail Clockwork PIP helper originally created `level.pintoscreenedge` with `level.player newpip()`. That bound the drill PIP camera to Logan even when the validated secondary local player performed the drill.

## Source changes in the local Stage 4 package

Updated `maps__clockwork_pip.gsc`:

- `pip_enable(player)` accepts an optional owner. Existing call sites that omit the argument preserve the stock fallback to `level.player`.
- If the same player already owns the PIP object, the helper stops any previous interference thread and disables the old state before reuse.
- If ownership changes, it notifies the previous PIP object, disables it, creates a new PIP object with `player newpip()`, and records `level.pintoscreenedge_player`.
- `pip_display_timer` accepts an optional eleventh `player` argument, falls back to Logan for legacy callers, and passes the selected player into both PIP initialization and corner setup.
- `pip_remove_l_corners` accepts the owner and uses `player.origin` when positioning the fallback PIP camera.

Updated the local Stage 4 `maps__clockwork_interior.gsc` call site to pass the selected `drill_player` into `pip_display_timer`.

## Validation

- Both local GSC files compile with GSC Tool 1.6.2.364 for IW6 / PS3.
- Both files decompile and recompile to byte-identical GSCBIN.
- A read-only audit tool checks the interior call site, selected-player `newpip` creation, previous-PIP thread cleanup, timer forwarding and owner-based fallback origin.
- Synthetic tests reject hard-coded primary-player PIP creation and missing owner forwarding.

## Runtime boundary

The PIP camera object is now created for the selected player in source. This does not prove that all `newhudelem` elements render independently in both split-screen viewports. That requires a live Campaign split-screen test on PS3/RPCS3. Native Player 2 creation, independent camera context and actual HUD visibility remain separate runtime gates.

No FastFile rebuild, signing or PKG creation is claimed by this phase.

## CI validation

GitHub Actions run `45071437477` passed all five jobs. The Python suite passed 202 tests, including the PIP-owner audit and negative tests for primary-player hard-coding. GSC Tool compile/round-trip, PS3 PRX bootstrap, protocol-header and worker checks passed. This is still source/build validation; no PS3/RPCS3 Campaign runtime test was performed.
