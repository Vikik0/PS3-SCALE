# Local controller input layer

## Purpose

Provide a small, testable input boundary for the two local player slots before connecting either slot to Ghosts Campaign.

## Current implementation

- Player slot 1 maps to PS3 controller port 0.
- Player slot 2 maps to PS3 controller port 1.
- The PS3 adapter checks port presence with `cellPadGetInfo2`.
- Each connected port is read independently with `cellPadGetData`.
- The decoder maps the two digital button words, both analog sticks, and L2/R2 pressure values.
- A per-port error mask distinguishes a missing controller from a read failure.
- Host tests cover mapping, analog data, two-port separation, disconnected ports, and invalid data lengths.

## Lifecycle rule

This code runs inside an already active game process. It must not call `cellPadInit` or `cellPadEnd`, because those functions manage a system service that Ghosts itself owns. The PRX must not intercept or replace the game's input path until the correct runtime hook and call order are verified.

## Current call path

The PRX exposes `ghc_local_input_refresh` and snapshot getters. These functions are compiled into the PRX but are not yet called from a game frame hook. The next integration stage must identify a safe per-frame point and verify that polling does not interfere with the title's own controller reads.

## PRX export surface

The PRX now publishes a named `ghosts_coop` export library. It exposes the manual input refresh function, poll result/count, connected and read-error masks, per-player connected state, button bits, stick axes, trigger pressure, and basic module/mission diagnostics.

The NIDs are recorded in `client/ps3/prx/exports.yml`, emitted in `exports.S`, and checked by the pinned PS3DK `nidgen verify` command. CI also generates `libghosts_coop_stub.a` for future diagnostic/importing PRXs. This validates the export/import metadata pair; it does not execute the functions on a console.

## Not yet verified

- Physical PS3 controller reads from inside Ghosts.
- Correct controller assignment in a live two-player local mode.
- Whether Campaign can retain two local-player slots.
- Independent Campaign camera, HUD, actor, weapon, health, AI and checkpoint behavior.

A passing host test or PRX build is not evidence that Campaign co-op is playable.
