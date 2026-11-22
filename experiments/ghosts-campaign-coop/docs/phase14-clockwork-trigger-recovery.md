# Phase 14 — Clockwork trigger ownership and death recovery

## Scope

This phase builds on the user-owned Stage 1 Clockwork mission source. The game-derived source and GSCBIN remain in the local Stage 2 package; this public repository contains only the audit logic and the non-source validation record.

## Changes in the local Stage 2 patch

- Capture the toucher directly from the trigger entity's `trigger` event instead of relying on the mission flag callback to carry the activator.
- Validate the captured entity as a living player. Accept the native secondary only when it matches `level.playengineeffects` and passes the split-screen identity checks; otherwise fall back to Logan.
- Set the mission start flag with the selected player as its payload and pass that same entity into `handle_drilling`.
- Remove the parallel `watch_for_drill_trigger` waiter and its shared activator variable.
- On death, clear the drill-start flag, allow the worker to finish its 50 ms cleanup cycle, then reopen the trigger for a retry.
- Remove the worker's redundant `flag_wait(var_1)`; it no longer blocks indefinitely when the start flag is cleared while the player is dead.
- Preserve the original `ammoCounterHide` dvar and restore it when the drill is no longer in use.

## Local build record

- GSC Tool: 1.6.2.364
- Target: IW6 / PS3
- Compile: passed
- Decompile: passed
- Recompile after decompile: passed
- Compiled GSCBIN: 22,902 bytes
- Stage 2 source SHA-256: `d2411ee69e328c6932f53ecc5db8e547bad465379a8cd9593fa1f171f0a5c73b`
- Compiled GSCBIN SHA-256: `1e09fbe59e6be0bf65f67bd1fea730972ee50980d75b6563ad726f637f39903b`
- Recompiled GSCBIN SHA-256: `1e09fbe59e6be0bf65f67bd1fea730972ee50980d75b6563ad726f637f39903b`
- Result: the first and second GSCBIN files are byte-identical.

## Runtime gates still open

- The exact MapEnts trigger class for the two drill spots is not confirmed. The Clockwork FastFile exposes the MapEnts pool entry as an unresolved placeholder, and the current read-only MapEnts scanner finds no entity-text candidate.
- Direct delivery of the trigger event to the new listener has not yet been confirmed in a live Campaign session.
- No FastFile rebuild, signing, or PKG creation has been performed.
- No PS3 or RPCS3 runtime test has been performed.
- The patch consumes a native secondary player if the engine has already created one. It does not create a second local client, independent camera, or independent HUD.

A successful GSC compile and byte-identical round-trip validate the bytecode format, not the in-game behavior. Do not treat this phase as an installable build.
