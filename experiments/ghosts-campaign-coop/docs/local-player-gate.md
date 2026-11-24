# Local-player readiness gate

## Purpose

Before any Campaign controls are enabled, the runtime adapter must prove that the engine has two real local players. The gate prevents a cloned/visual entity, duplicated client pointer, shared command stream, shared camera/HUD context, stale input sample, wrong controller mapping or wrong primary-player identity from being accepted as Player 2.

## Required observations

For both local slots, the adapter must supply verified observations for:

- split-screen mode active;
- the player is recognized by the engine as a split-screen player;
- primary/secondary identity;
- controller port and a connected, valid input frame;
- both observations captured in the same engine frame;
- local-client number, client ID and entity ID;
- distinct `gclient`, `gentity` and player-state addresses;
- a distinct native input-command buffer for each player;
- independent view/refdef/camera contexts;
- independent HUD contexts;
- campaign role: Player 1 is Logan; Player 2 is a separate guest soldier.

The local-client number, client ID, entity ID and controller port are separate values. Do not assume they are interchangeable. The gate checks that the local-client numbers are present and unique, while controller ports must map to the expected physical ports 0 and 1.

## Fail-closed behavior

`ghc_local_player_gate_validate()` is a pure C function. It does not read memory, allocate an entity or call an engine function. It accepts only observations collected by a future verified adapter.

The gate returns a specific failure status and slot for missing or contradictory data. It returns `GHC_LOCAL_GATE_READY` only if both players have valid input observations from the same engine frame, unique IDs, distinct player-state structures, distinct command buffers, and separate view and HUD contexts. Any failure keeps the local co-op adapter disabled.

The address checks compare the two players' engine-owned objects across categories. A command buffer, view context or HUD context for Player 2 may not alias any corresponding or cross-category object for Player 1. These are identity checks, not a substitute for validating the actual structure layouts and ownership in the exact TU 1.16 runtime.

## Current status

The gate and its host tests are implemented and compiled into the PRX build. The strengthened host contract now rejects missing/stale input frames, mismatched observation frames, shared command buffers, shared view contexts and shared HUD contexts. It is not yet called by the PRX because no Campaign frame hook or verified local-player observations exist. Passing tests prove only the validation logic, not that Ghosts currently supplies the required observations.
