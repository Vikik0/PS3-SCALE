# Crash Bash Enhanced — Cross-platform architecture

## Product target

A PS3 player and a browser player must run the same Crash Bash match, not merely share a lobby. The browser player does not need a PS3, but must provide their own legally obtained Crash Bash disc image. The project does not host, bundle, or redistribute the game's ISO, BIOS, music, textures, or other proprietary game assets.

## Selected architecture

- **PS3 client:** the original Crash Bash disc running in the PS1 emulator on PS3, with the project PRX translating remote controller state into virtual PS1 pad slots.
- **Browser client:** EmulatorJS with its PlayStation (PCSX-ReARMed) core. The browser player loads the user's disc image and optional BIOS locally, captures its local pad as a CBN1 seven-byte input record, and applies the host's authoritative four-pad records to the emulator.
- **Identity and rooms:** guest sessions and room codes. PSN authentication is not used.
- **Game transport:** a room-scoped WebSocket relay. The browser cannot open arbitrary UDP sockets, so its CBN1 lockstep client sends packets through the same CBU1/CBR1 relay protocol as PS3. The PS3 adapter intercepts the upstream UDP socket calls and translates them to those relay messages.
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

The browser runtime uses EmulatorJS with its PlayStation (PCSX-ReARMed) core rather than compiling the upstream SDL_GPU desktop renderer to WebAssembly. It loads the user's disc image and optional BIOS as browser File objects; neither is uploaded to the Worker. The browser exposes pad-apply and frame-count hooks, captures the local controller through EmulatorJS's GameManager, and runs a CBN1 protocol-2 joiner. The client handles JOIN/WELCOME, PING/PONG, empty SAVE slots, START/READY, INPUT retransmission/acknowledgements, and authoritative RECORD packets. It gates the emulator in two-frame steps and applies all four pad states from each host record. The authenticated WSS transport batches CBU1 packets every 220 ms and decodes CBR1 envelopes. Browser tests cover the packet codec, controller mapping, lobby handshake, and frame-record flow. This is an integration prototype: live timing, PAL behavior, RAM fingerprints, and physical PS3/browser compatibility remain unverified. Browser networking does not expose raw UDP sockets.

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
- Browser CBN1 lockstep joiner with host handshake, empty-save acknowledgements, input-history retransmission, authoritative four-pad records, and two-frame EmulatorJS stepping, covered by unit tests.
- An installable PS3 homebrew PKG wrapper for the input PRX and VSH loader; CI validates its PKG header and content ID. Hardware installation and runtime behavior are not yet verified.

### Still required before a public playable release
1. Configure Cloudflare deployment credentials and deploy the Worker plus Vite lobby; CI currently skips deployment when the Cloudflare secrets are absent.
2. Install the generated PKG on physical PS3 hardware and verify the VSH loader, process injection, and in-game host/join menu (SELECT + L3 + R3).
3. Run the first real PS3-host/browser-joiner match using matching NTSC disc revisions and verify the CBN1 handshake, save-slot acknowledgements, start barrier, input routing, and two-frame stepping.
4. Measure real RTT and packet loss, validate the fixed 12-step delay, and test disconnect/rejoin, host loss, slow tabs, and long matches.
5. Add browser-side fingerprints or another desync detector; the current browser sends `print_step = -1`, so it does not yet compare emulator state against the PS3 host.
6. Validate PAL 50 Hz timing separately. The current PS3 adapter and browser frame gate are tuned around the 30-step/s NTSC path.

**Status:** the browser CBN1 joiner is now connected in source to the authenticated WSS relay and EmulatorJS frame/input hooks, and CI passes its 22 web tests, Worker integration tests, PS3 WSS tests, PRX builds, and PKG verification. The PS3 adapter and browser protocol are still an unverified prototype: Cloudflare deployment credentials are missing, the PKG has not been installed on hardware, and no real PS3-to-browser match has been completed. Do not treat this branch as release-ready.
