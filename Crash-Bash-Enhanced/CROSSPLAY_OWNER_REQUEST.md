# Request for supported Crash Bash browser/PS3 cross-play integration

**Purpose:** request written permission and an official technical interface for a non-browser PS3 client to join the same rooms and matches as browser players.

The PS3 project currently uses RetroArch + PCSX-ReARMed and its own Netplay transport. The browser service uses a separate authoritative simulation and input protocol. A room code does not make those transports compatible.

## Permission requested

Please confirm whether the site owner permits:

1. A third-party PS3 homebrew client to connect to the service.
2. A non-browser client to authenticate, create/join rooms, select a character and mark Ready.
3. The PS3 client to send controller inputs through an owner-approved interface.
4. Distribution of the approved client to users.

No private browser client, game assets, or undocumented wire protocol will be copied or reverse-engineered.

## Technical interface requested

Please provide an official SDK, source access, or documented API covering:

- Guest identity, session creation and authentication.
- Public/private room discovery, room creation, invite-code joining and seat assignment.
- Character selection, Ready state and match-start lifecycle.
- The authoritative 25 Hz input message schema, sequence numbers, acknowledgements and timing rules.
- Server-to-client state snapshots/deltas and deterministic simulation requirements.
- Protocol/version negotiation and compatibility policy.
- Disconnect, reconnect, timeout and room cleanup behavior.
- Rate limits, abuse prevention and security requirements.
- A staging server and test credentials that do not affect production players.

## Acceptance test proposed

1. One PS3 client and one browser client enter the same owner-approved test room.
2. Both complete character selection and occupy distinct seats.
3. Inputs from both devices are applied by the authoritative simulation.
4. A full match completes without state divergence.
5. Disconnect/rejoin follows the published room rules.
6. Two PS3 clients and two browser clients can fill all four seats.
7. The owner approves the implementation and release plan.

## Current technical boundary

Until written permission and a supported interface are supplied, this project will keep PS3-to-PS3 play on RetroArch Netplay and will not claim compatibility with the browser service.
