# Local controller input layer

## Purpose

Provide a small, testable input boundary for the two local player slots before connecting either slot to Ghosts Campaign.

## Current implementation

- Player slot 1 maps to PS3 controller port 0.
- Player slot 2 maps to PS3 controller port 1.
- The PS3 adapter checks port presence with `cellPadGetInfo2`.
- Each connected port is read independently with `cellPadGetData`.
- The decoder maps digital buttons, both analog sticks, and L2/R2 pressure values.
- A per-port error mask distinguishes a missing controller from a failed read.
- Host tests cover mapping, analog data, two-port separation, disconnected ports, and invalid data lengths.

## Per-frame input processing

The portable tracker turns each raw snapshot into a stable per-frame input record:

- `buttons`: buttons currently held.
- `pressed`: buttons that changed from released to held in this frame.
- `released`: buttons that changed from held to released in this frame.
- Analog sticks are normalized to signed -127..127 values with a configurable radial-independent axis deadzone (default 20).
- Trigger pressure is preserved as 0..255.
- A confirmed disconnect emits one release edge for held buttons, then clears the prior state.
- A read error is marked invalid and preserves the last known player state; it does not fabricate button releases.

The tracker is independent for each player slot and uses no PS3 SDK calls, so its behavior is host-testable.

## Lifecycle rule

This code runs inside an already active game process. It must not call `cellPadInit` or `cellPadEnd`, because those functions manage a system service that Ghosts itself owns. The PRX must not intercept or replace the game's input path until the correct runtime hook and call order are verified.

## Current call path

The PRX exposes `ghc_local_input_refresh` and diagnostic getters. Refresh reads the two PS3 ports and updates the portable frame tracker. The function is still manual and is not called from a verified game frame hook. It has not been tested on physical hardware.

The exported getters now expose:
- connected, valid and read-error masks;
- held, newly pressed and newly released button bitsets for each player;
- raw and normalized stick axes;
- L2/R2 pressure values.

## Not yet verified

- Physical PS3 controller reads from inside Ghosts.
- Whether the game's own `cellPadGetData` polling conflicts with an additional read.
- Correct controller assignment in a live two-player local mode.
- Whether Campaign can retain two local-player slots.
- Independent Campaign camera, HUD, actor, weapon, health, AI and checkpoint behavior.

A passing host test or PRX build is not evidence that Campaign co-op is playable.
