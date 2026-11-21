
# PS3 PRX bootstrap

This folder builds the first PS3 PRX artifact for the project.

## Phase 1 purpose

The first playable target is local Campaign co-op on one PS3. The PRX must eventually:
- identify the exact Ghosts Title ID and TU profile;
- install only verified runtime hooks for the matching executable;
- preserve the original Logan Campaign flow;
- enable/reuse the second local-player slot and controller path;
- keep player input, camera and HUD state independent;
- connect Campaign health, revive and checkpoint behavior only after the player-two path is proven.

## Parked online components

The current build also compiles GHC2, GCP1, WebSocket, HTTPS lobby, STUN, UDP and direct-P2P components. They are retained for Phase 2 but are not part of the Phase 1 runtime design. Do not start the lobby or network transport in the local build.

## Explicit limitation

The current module is a **passive bootstrap only**. It does not enable a second local player, install Campaign hooks, change the camera, read controller 2 or modify game memory. The existing network and game payload code is source/CI validated only. Do not install it as a playable Campaign co-op mod and do not treat the build as proof that local co-op works.

The next game-specific step is to compare the exact TU 1.16 local two-player mode runtime with Campaign, then verify candidate local-player initialization functions against the exact title ID and CEX executable. Public multiplayer offsets are not safe to call blindly.

## Build

GitHub Actions downloads the pinned PS3DK PPU toolchain and host tools, compiles the PRX runtime CRT, then runs the Makefile. The unsigned PRX and fake-signed SPRX are CI artifacts, not committed binaries.
