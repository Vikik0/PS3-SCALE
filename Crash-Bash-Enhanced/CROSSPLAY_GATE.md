# Cross-play integration gate: crashbash.app

**Status: blocked pending permission and a supported integration interface.**

## What is confirmed

The public Crash Bash Online site is a browser port with its own game simulation, not a PS1 emulator. Its published technical notes say that the simulation runs at 25 Hz, the server is authoritative, and clients send controller inputs rather than player positions.

The PS3 package in this repository is a different architecture: RetroArch with PCSX-ReARMed runs the user's own PS1 disc dump and uses RetroArch Netplay to synchronize PS3 clients. The two multiplayer systems do not share a protocol.

The public site documentation does not publish a third-party client API, SDK, room wire protocol, or supported console integration. Its Terms also say not to script the game or copy its data. We must not reverse-engineer, automate, scrape, or impersonate its private client.

## What is needed from the site owner

Before implementing a PS3-to-browser client, obtain written permission and a supported technical interface covering:

1. Guest identity and session registration for a non-browser client.
2. Room discovery, public matchmaking, private room creation, and room-code joining.
3. Character selection, seat assignment, ready state, and disconnect/rejoin behavior.
4. The exact 25 Hz input message format, sequencing, acknowledgement, and timing rules.
5. Server-to-client state updates, deterministic simulation requirements, version compatibility, and error handling.
6. Authentication, rate limits, abuse controls, and a staging/test server.
7. Permission to distribute a PS3 client that interoperates with the site.

A room code alone is not sufficient. Reusing a code from one service does not make its gameplay transport compatible with another.

## Implementation paths after approval

### Path A — supported site client on PS3

If the owner supplies source or a supported SDK, port the site's authorized client/runtime to PS3 and use its documented room and simulation protocol. This is the closest route to playing in the same rooms as browser users.

### Path B — owner-supported input gateway

If the owner supplies an authenticated input API and a compatibility contract, build a small gateway that translates PS3 controller events to the documented server input format. This still requires a PS3-side simulation/rendering path that matches the site's authoritative game.

### Path C — independent PS3 Netplay

Until A or B is approved and technically specified, keep the current PS3 package on RetroArch Netplay. This supports PS3-to-PS3 sessions only; it is not cross-play with crashbash.app.

## Release acceptance tests for cross-play

Do not mark cross-play complete until all of these pass on real hardware:

- One PS3 client and one browser client join the same site-owned room.
- They can complete character selection and start a battle together.
- Inputs from both devices are reflected correctly through the authoritative 25 Hz simulation.
- The match remains synchronized through a full round on a representative set of arenas.
- A disconnect and rejoin behaves according to the site's room rules.
- Two PS3 clients and two browser clients can occupy all four seats together.
- The site owner approves the integration and its security model.

## Current project status

- PS3-to-PS3: RetroArch Netplay is configured and the PKG build is covered by CI. Physical-console gameplay and latency tests remain required.
- Four-seat web lobby: public/private rooms, invite-code joins, ANY BATTLE, guest tokens, heartbeats, and signaling are implemented as a separate lobby prototype.
- Shared gameplay between that lobby, the PS3 package, and crashbash.app: **not implemented**.
