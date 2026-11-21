# Crash Bash Enhanced — Cross-platform architecture

## Product target

A PS3 player and a browser player must run the same Crash Bash match, not merely share a lobby. The browser player does not need a PS3, but must provide their own legally obtained Crash Bash disc image. The project does not host, bundle, or redistribute the game's ISO, BIOS, music, textures, or other proprietary game assets.

## Selected architecture

- **PS3 client:** the original Crash Bash disc running in the PS1 emulator on PS3, with the project PRX translating remote controller state into virtual PS1 pad slots.
- **Browser client:** EmulatorJS with its PlayStation (PCSX-ReARMed) core. The browser player loads the user's disc image and optional BIOS locally, then exposes a four-player input hook for the CBN1 adapter.
- **Identity and rooms:** guest sessions and room codes. PSN authentication is not used.
- **Game transport:** a room-scoped WebSocket relay. The browser cannot open arbitrary UDP sockets, so its upcoming CBN1 adapter must use the same relay protocol as PS3. The PS3 adapter already intercepts the upstream UDP socket calls and translates them to CBU1/CBR1 relay messages.
- **Hosting:** static browser files served by the same Cloudflare Worker origin as the API and SQLite-backed Durable Object. No home PC or paid database is part of the target architecture.

## Relay packet format — CBR1

The client-to-relay uplink supports raw legacy packets and CBU1 batches. CBU1 is `magic[4] = CBU1`, `count[1]`, then `count` entries of `recipient_session_id[16]`, `payload_length[2, big-endian]`, and `payload[1–1200]`; an all-zero recipient ID broadcasts, otherwise the relay routes only to that peer. One WebSocket message can contain up to 32 datagrams and is capped at 28,672 bytes. The relay forwards each datagram as an authenticated CBR1 sender envelope:

| Offset | Bytes | Meaning |
| --- | ---: | --- |
| 0 | 4 | ASCII magic CBR1 |
| 4 | 16 | Sender session ID, decoded from the 32-character hex session ID |
| 20 | 2 | Payload length, unsigned big-endian |
| 22 | 1–1200 | Opaque game packet |

The server obtains the sender ID from the authenticated guest session, not from a client-supplied field. The browser authenticates with the guest token in the `cbash-auth.<token>` subprotocol. The PS3 uses `POST /v1/rooms/:code/join-guest` and receives a 60-second, single-use ticket for the `cbash-ticket.<ticket>` subprotocol; the long-lived guest token is not returned to the PS3 client. Closing the last socket removes that player from the room; browser reconnect first re-joins with its guest token, while a PS3 client can request a fresh one-time ticket. Only the `PING` text keepalive is accepted; other text messages are rejected. The relay caps each connected player at five inbound WebSocket messages per second, counting binary batches and text controls, and limits each opaque datagram to 1200 bytes. The upstream `net_step()` calls `send_news()` every 30–33 ms, and the host can send a separate records packet to each joiner. The PS3 adapter now queues those datagrams, replaces superseded retransmissions for the same peer and packet type, and flushes a CBU1 batch every 220 ms. This preserves per-peer routing while staying below the message cap; a fixed 12-step input delay is used to cover the batching interval. The rate/latency trade-off is still a prototype and requires real-network testing. `PER_PACKET` limits records per datagram; it does not limit how often `send_news()` runs.

## Free-tier budget

The free plan is not unlimited. Cloudflare's Durable Objects pricing lists 100,000 requests per day and applies a 20:1 billing ratio to incoming WebSocket messages. Hard caps protect the free budget: 16 total browser and PS3 sessions, four open rooms, and one active match at a time. One four-player match held at the five-message-per-second cap uses up to 86,400 metered request-equivalents per day (4 × 5 × 86,400 ÷ 20), before setup and reconnect overhead. The 220 ms PS3 batch cadence is designed to remain below that per-socket cap; actual billing must be checked against deployed traffic. The service must reject excess traffic rather than enable paid usage.

References:
- Cloudflare Workers pricing: https://developers.cloudflare.com/workers/platform/pricing/
- Durable Objects limits: https://developers.cloudflare.com/durable-objects/platform/limits/

## Browser runtime and input bridge

The browser runtime now uses EmulatorJS with its PlayStation (PCSX-ReARMed) core instead of compiling the upstream SDL_GPU desktop renderer to WebAssembly. This avoids writing a separate WebGL 2 renderer. The local browser player loads the user's disc image and optional BIOS as browser File objects, without uploading either to the Worker. It exposes `CBASH_APPLY_PAD_STATE(slot, buttons, sticks)` and `CBASH_GET_EMULATOR_FRAME()` for the network adapter. A browser WSS transport now implements authenticated `cbash-v1` connections, CBU1 batching, CBR1 decoding, a 220 ms flush cadence, and PING/PONG keepalives; framing and controller mapping have unit tests. The remaining browser work is to connect the upstream CBN1 lockstep state machine to that transport and drive EmulatorJS pad/frame hooks. Browser networking does not expose raw UDP sockets.

References:
- EmulatorJS project and supported systems: https://github.com/EmulatorJS/EmulatorJS
- EmulatorJS 4.2.3 GameManager input API: https://github.com/EmulatorJS/EmulatorJS/blob/v4.2.3/data/src/GameManager.js
- EmulatorJS CDN/versioning: https://github.com/EmulatorJS/EmulatorJS#cdn-integration
- EmulatorJS license and third-party notice: ../web/THIRD-PARTY-NOTICES.md

## Implementation status

### Implemented in this branch
- Guest sessions without PSN.
- Four-player room creation, joining, ready state, and host start.
- Host handoff and room cleanup.
- A prototype binary WebSocket room relay with authenticated sender envelopes, packet-size checks, and per-client rate limits.
- CI coverage for lobby behavior, relay framing/rate limits, a certificate-verified TLS 1.2/WSS PS3 client, and PRX builds.
- A development PS3 host/join/start menu wired to the relay APIs.
- A local EmulatorJS browser player with user-owned CUE/BIN/CHD/PBP/ISO/IMG loading, optional local BIOS selection, and a four-player PS1-to-libretro input hook with unit tests.
- Browser-side authenticated WSS relay transport with CBU1 batching and CBR1 envelope decoding, covered by unit tests.
- An installable PS3 homebrew PKG wrapper for the input PRX and VSH loader; CI validates its PKG header and content ID. Hardware installation and runtime behavior are not yet verified.

### Still required before cross-platform play
1. Connect the upstream CBN1 lockstep state machine to the browser WSS transport, then feed each synchronized pad record into the EmulatorJS input hook at the correct emulator frame.
2. Connect the browser input hook to a WebSocket-backed CBN1 lockstep adapter, using the same per-peer relay routing as the PS3 client.
3. Validate the PS3 lockstep-to-CBU1/CBR1 shim and browser adapter under packet loss, high RTT, disconnects, and reconnects; tune the fixed relay input delay from measured results.
4. Configure Cloudflare deployment credentials; the deployment workflow now builds the Vite lobby, deploys the static assets and Worker together, and checks the public /health endpoint. The PS3 client now uses certificate-verified HTTPS/WSS; local TLS integration tests cover its handshake and relay protocol.
5. Install the generated PKG on physical PS3 hardware and verify the VSH loader, process injection, and in-game host/join menu (SELECT + L3 + R3).
6. Test matching builds, input delay, disconnect/rejoin, and deterministic frame fingerprints on a real PS3 and a real browser.

**Status:** the PS3 adapter routes upstream lockstep datagrams through the relay in code, and its client now uses TLS 1.2/WSS with certificate verification. The installable PS3 PKG now builds in CI and its package header/content ID are validated. The local EmulatorJS browser runtime is present, but its CBN1 input adapter is not connected yet. Cloudflare deployment credentials and physical hardware/browser match tests are also still missing. This branch is not yet cross-platform playable.
