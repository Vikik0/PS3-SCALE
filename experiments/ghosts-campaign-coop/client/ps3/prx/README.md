
# PS3 PRX bootstrap

## Phase 1 purpose

The first playable target is local Campaign co-op on one PS3:
- two local DualShock 3 controllers;
- Player 1 remains Logan;
- Player 2 is a separate Multiplayer-style soldier;
- no internet, PSN, lobby or server dependency.

## Implemented groundwork

- Passive PRX lifecycle/bootstrap.
- Standalone downed/revive state machine.
- Host-tested controller-data decoder.
- PS3 `sys_io` adapter for controller ports 0 and 1 using `cellPadGetInfo2` and `cellPadGetData`.
- Portable per-frame tracker for pressed/released edges, normalized stick axes, trigger pressure and safe read-error handling.
- Diagnostic PRX exports for raw input and per-frame input values.

The PRX does **not** call `cellPadInit` or `cellPadEnd`; the running game owns that subsystem lifecycle. It does not change the game's controller routing.

## Explicit limitation

The current PRX is not a playable Campaign co-op mod. No Campaign player-two slot, actor spawn, input hook, split-screen camera, HUD integration or mission adapter is implemented. The input poll function is a diagnostic building block; it is not yet called from a game frame hook. No live PS3 hardware test has been performed.

The PS3 adapter imports `sys_io` functions through the PS3DK `libio_stub` archive. Successful linking validates import metadata only; it does not prove that the in-game module can read both ports on every Ghosts executable.

## Build

GitHub Actions downloads the pinned PS3DK PPU toolchain and host tools, builds the required system import stubs including `libio_stub`, then compiles and validates the PRX. The unsigned PRX and fake-signed SPRX are CI artifacts, not committed binaries.
