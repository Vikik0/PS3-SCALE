# PS3 PRX bootstrap

This folder builds the first PS3-loadable PRX artifact for the project.

## Current purpose

- Compile the shared GHC2 packet codec for the PPU.
- Compile the RFC 6455 client binary-frame encoder and server frame parser for the PPU.
- Compile the PS3 libhttp/libssl guest lobby client and JSON response parser.
- Compile RFC 5389 STUN binding request/response handling and PS3 libnet UDP socket helpers.
- Link the PS3 net/http/http-util/ssl/sysmodule import stubs required by the lobby client.
- Link a valid Lv-2 PRX with the PS3DK PRX CRT/linker script.
- Run `prx-gen check` on the result.
- Produce a fake-signed `ghosts_coop.fake.sprx` for the CFW development target.

## Explicit limitation

The current module is a **passive bootstrap only**. The GHC2 and WebSocket frame codecs, HTTPS REST lobby client, STUN codec and PS3 libnet UDP socket helpers are compiled into it. The lobby client is not started automatically and has not been verified on a console. Live STUN/NAT discovery, WebSocket socket lifecycle, Call of Duty: Ghosts hooks, Player 2 spawn, Campaign memory modification and gameplay replication are not active. Do not install it as a gameplay mod and do not treat the build as proof that Campaign co-op works.

The next game-specific step requires the exact user-owned Ghosts executable and update identity, followed by offline validation of the candidate spawn functions in that exact build. The public TU 1.16 offsets are not safe to call blindly.

## Build

The GitHub Actions job downloads the pinned PS3DK PPU toolchain and host tools, compiles the PRX runtime CRT, then runs this Makefile. The unsigned PRX and fake-signed SPRX are CI artifacts, not committed binaries.
