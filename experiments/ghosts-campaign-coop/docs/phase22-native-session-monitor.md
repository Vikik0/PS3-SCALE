# Phase 22 — Native Campaign player-pair lifecycle monitor

## Purpose

The existing local-session bridge only revalidated the native primary/secondary pair immediately before Campaign actions. If a controller disconnected while neither player was interacting with a drill or loadout, the Campaign aliases could remain stale until the next action.

Phase 22 adds a single guarded monitor that starts after a successful native-pair bind or a valid idempotent session reuse.

## Behavior

- \`ghc_campaign_start_session_monitor()\` uses a level-scoped guard to avoid starting duplicate monitor threads.
- \`ghc_campaign_watch_native_session()\` rechecks the exact native player pair and the Campaign aliases every 250 ms.
- A successful check resets the consecutive-failure counter.
- Seven consecutive failed checks are tolerated. The eighth failure (approximately two seconds) calls \`ghc_campaign_invalidate_local_session()\`.
- Central invalidation releases both active drill locks, sets session status 8, clears the stale secondary alias and native-player handles, and preserves \`level.player\` only if the stored primary still reports a valid native primary identity.
- The monitor clears its active guard when it exits.

The two-second debounce is intended to avoid invalidating a valid session because of a transient one-frame entity-array transition. It is a source-level policy and still needs runtime timing validation.

## Explicit non-goals

The monitor does **not**:
- create a local client, controller binding, player entity, camera, or HUD;
- clone or respawn a player;
- automatically rebind a replacement player pair;
- write game memory or install a PPU hook.

After status 8, the bridge's \`main()\` must be called again to run its existing bounded native-pair discovery and bind a replacement pair. The monitor only owns validation and cleanup; it does not claim native Player 2 creation.

## Validation

- Five new source-contract tests cover monitor startup, duplicate-thread guard, alias/entity revalidation, two-second debounce, counter reset, cleanup and absence of player-spawn/rebind calls.
- The bridge remains subject to the pinned GSC Tool 1.6.2.364 compile/decompile round-trip and manifest hash checks.
- Host tests and GSC compile prove source/build consistency only. They do not prove that the monitor is loaded by the retail Campaign VM or that a second local player exists.

## Current native gate

The underlying native Campaign local-client creation path remains unresolved. The verified GSC evidence still shows that Campaign's shared loader enumerates already-existing player entities and binds the second alias only when the engine has supplied a second split-screen player. Multiplayer GSC registers an engine-created player after its native connect/begin lifecycle; the scanned scripts do not create the local-client slot or controller-port mapping.

No decrypted TU 1.16 PPU runtime image or live two-controller Campaign capture is available in this work session. Do not infer a hook address, copy a Multiplayer address into Campaign, or mark this as playable co-op.
