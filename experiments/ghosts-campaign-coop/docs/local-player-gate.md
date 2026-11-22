# Local-player readiness gate

## Purpose

Before any Campaign controls are enabled, the runtime adapter must prove that the engine has two real local players. The gate prevents a cloned/visual entity, duplicated client pointer, wrong controller mapping or wrong primary-player identity from being accepted as Player 2.

## Required observations

For both local slots, the adapter must supply verified observations for:

- split-screen mode active;
- the player is recognized by the engine as a split-screen player;
- primary/secondary identity;
- controller port and a connected input frame;
- local-client number, client ID and entity ID;
- distinct `gclient`, `gentity` and player-state addresses;
- campaign role: Player 1 is Logan; Player 2 is a separate guest soldier.

The local-client number, client ID and entity ID are separate values. Do not assume they are interchangeable.

## Fail-closed behavior

`ghc_local_player_gate_validate()` is a pure C function. It does not read memory, allocate an entity or call an engine function. It accepts only observations collected by a future verified adapter.

The gate returns a specific failure status and slot for missing or contradictory data. It returns `GHC_LOCAL_GATE_READY` only if both slots pass and all identifiers/state addresses are distinct. Any failure keeps the local co-op adapter disabled.

## Current status

The gate and its host tests are implemented and compiled into the PRX build. It is not yet called by the PRX because no Campaign frame hook or verified local-player observations exist. Its passing tests prove only the validation logic, not that Ghosts currently supplies the required observations.
