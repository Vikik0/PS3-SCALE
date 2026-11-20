# Ghosts Campaign Co-op — Prototype 0.2

This is an isolated research experiment for a two-player cooperative mode in the original PS3 Campaign of Call of Duty: Ghosts.

## Target profile

- Console: PS3 CEX CFW 4.93
- Game update: Title Update 1.16
- Maximum players: 2
- Player 1 remains Logan and controls the original story flow.
- Player 2 is intended to be an additional Multiplayer-style soldier actor.
- Story NPCs such as Hesh must remain untouched.

## Prototype status

- Guest lobby/session prototype: implemented and host-tested.
- Six-character room codes, two-player cap and ready/start flow: implemented and tested.
- UDP peer relay: implemented; local two-client UDP integration test passes.
- Regional title-ID registry: implemented as metadata; profiles are untested.
- Candidate TU 1.16 spawn primitives documented for offline research only.
- PS3 client plugin, Campaign actor spawn, game-state replication, PSN identity and PKG/EBOOT/SPRX: not implemented.

## Compatibility policy

The project will not claim one universal EBOOT/SPRX. It selects profiles by title ID, title update, firmware family and firmware version. Known regional IDs are registered in docs/compatibility-matrix.md. The first target is CEX 4.93 + TU 1.16; DEX, HEN, other CEX versions and all title-ID variants require separate validation.

## Verification

Prototype 0.2 contains 16 passing Python unit and UDP integration tests. They validate the lobby state machine, protocol envelope, local UDP relay and compatibility registry. They do not validate a second player in Campaign.

## Important

No retail game files, copyrighted game assets, EBOOT, SPRX or installable PKG are included. The Campaign co-op feasibility gate remains open until a second actor can be spawned safely in a user-owned game build and tested on real hardware. The source tree is maintained on this isolated branch, separate from PS3-SCALE main.
