# PS3 PRX bootstrap

This folder builds the first PS3-loadable PRX artifact for the project.

## Current purpose

- Compile the shared GHC2 packet codec for the PPU.
- Compile the RFC 6455 client binary-frame encoder and server frame parser for the PPU.
- Link a valid Lv-2 PRX with the PS3DK PRX CRT/linker script.
- Run `prx-gen check` on the result.
- Produce a fake-signed `ghosts_coop.fake.sprx` for the CFW development target.

## Explicit limitation

The current module is a **passive bootstrap only**. The GHC2 and WebSocket frame codecs are compiled into it, but no HTTPS/TLS session client, WebSocket socket lifecycle, Call of Duty: Ghosts hook, Player 2 spawn, Campaign memory modification or gameplay replication is active. Do not install it as a gameplay mod and do not treat the build as proof that Campaign co-op works.

The next game-specific step requires the exact user-owned Ghosts executable and update identity, followed by offline validation of the candidate spawn functions in that exact build. The public TU 1.16 offsets are not safe to call blindly.

## Build

The GitHub Actions job downloads the pinned PS3DK PPU toolchain and host tools, compiles the PRX runtime CRT, then runs this Makefile. The unsigned PRX and fake-signed SPRX are CI artifacts, not committed binaries.
