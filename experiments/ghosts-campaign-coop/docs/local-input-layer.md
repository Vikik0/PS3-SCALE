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

## Not yet verified

- Physical PS3 controller reads from inside Ghosts.
- Correct controller assignment in a live two-player local mode.
- Whether Campaign can retain two local-player slots.
- Independent Campaign camera, HUD, actor, weapon, health, AI and checkpoint behavior.

A passing host test or PRX build is not evidence that Campaign co-op is playable.
