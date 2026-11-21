# Crash Bash Enhanced — Cross-platform architecture

## Product target

A PS3 player and a browser player must run the same Crash Bash match, not merely share a lobby. The browser player does not need a PS3, but must provide their own legally obtained Crash Bash disc image. The project does not host, bundle, or redistribute the game's ISO, BIOS, music, textures, or other proprietary game assets.

## Selected architecture

- **PS3 client:** the original Crash Bash disc running in the PS1 emulator on PS3, with the project PRX translating remote controller state into virtual PS1 pad slots.
- **Browser client:** a WebAssembly build of the community port, with a browser-compatible renderer and a local file picker for the player's own disc image.
- **Identity and rooms:** guest sessions and room codes. PSN authentication is not used.
- **Game transport:** a room-scoped WebSocket relay. The browser cannot open arbitrary UDP sockets, so the browser and PS3 clients must both speak the relay protocol. The existing PS3 UDP adapter is not yet connected to this relay.
- **Hosting:** static browser files plus Cloudflare Workers and SQLite-backed Durable Objects. No home PC or paid database is part of the target architecture.

## Relay packet format — CBR1

The client-to-relay uplink supports raw legacy packets and CBU1 batches. CBU1 is `magic[4] = CBU1`, `count[1]`, then `count` entries of `recipient_session_id[16]`, `payload_length[2, big-endian]`, and `payload[1–1200]`; an all-zero recipient ID broadcasts, otherwise the relay routes only to that peer. One WebSocket message can contain up to 32 datagrams and is capped at 24,576 bytes. The relay forwards each datagram as an authenticated CBR1 sender envelope:

| Offset | Bytes | Meaning |
| --- | ---: | --- |
| 0 | 4 | ASCII magic CBR1 |
| 4 | 16 | Sender session ID, decoded from the 32-character hex session ID |
| 20 | 2 | Payload length, unsigned big-endian |
| 22 | 1–1200 | Opaque game packet |

The server obtains the sender ID from the authenticated guest session, not from a client-supplied field. The browser authenticates with the guest token in the `cbash-auth.<token>` subprotocol. The PS3 uses `POST /v1/rooms/:code/join-guest` and receives a 60-second, single-use ticket for the `cbash-ticket.<ticket>` subprotocol; the long-lived guest token is not returned to the PS3 client. Closing the last socket removes that player from the room; browser reconnect first re-joins with its guest token, while a PS3 client can request a fresh one-time ticket. Only the `PING` text keepalive is accepted; other text messages are rejected. The initial relay prototype caps each connected player at five inbound WebSocket messages per second (including keepalive pings) and 1200 payload bytes per message. Important traffic constraint: the upstream `net_step()` calls `send_news()` on every game step (about every 30–33 ms), and the host may send a separate records packet to each joiner. The current five-message-per-second relay cap therefore cannot carry unmodified gameplay traffic. CBU1 and a tested seven-frame PS3 batching queue now provide the protocol and scheduler, but the PS3 net adapter does not yet feed upstream per-frame UDP datagrams into that queue. The old estimate of roughly one packet per second was incorrect: `PER_PACKET` limits how many unacknowledged records fit in a packet, but does not reduce how often `send_news()` is called. Before play, implement targeted peer routing plus a batching shim that aggregates several lockstep datagrams per WebSocket message while preserving packet order and per-peer delivery.

## Free-tier budget

The free plan is not unlimited. Cloudflare's current Durable Objects pricing lists 100,000 requests per day and applies a 20:1 billing ratio to incoming WebSocket messages. Hard caps protect the free budget: 16 guest sessions, four open rooms, and one active match at a time. One four-player match held at the five-message-per-second cap uses up to 86,400 metered request-equivalents per day (4 × 5 × 86,400 ÷ 20), before setup and reconnect overhead. This budget calculation describes the prototype cap, not yet the game's actual traffic. Unbatched upstream traffic would exceed the free-tier budget; batching and strict session/time quotas are required to keep the service free. The service must reject excess traffic rather than enable paid usage.

References:
- Cloudflare Workers pricing: https://developers.cloudflare.com/workers/platform/pricing/
- Durable Objects limits: https://developers.cloudflare.com/durable-objects/platform/limits/

## Browser-port constraints

The upstream desktop renderer uses SDL_GPU. SDL's own development FAQ currently says SDL_GPU does not support the web platform, so a browser build needs a WebGL 2 renderer (or another browser-supported rendering path); simply compiling the existing desktop target to WebAssembly is not sufficient. Emscripten provides a WebSocket API, but browser networking does not expose raw UDP sockets.

References:
- SDL3 development FAQ: https://wiki.libsdl.org/SDL3/FAQDevelopment
- Emscripten networking: https://emscripten.org/docs/porting/networking.html
- SDL3 Emscripten guide: https://wiki.libsdl.org/SDL3/README-emscripten

## Implementation status

### Implemented in this branch
- Guest sessions without PSN.
- Four-player room creation, joining, ready state, and host start.
- Host handoff and room cleanup.
- A prototype binary WebSocket room relay with authenticated sender envelopes, packet-size checks, and per-client rate limits.
- CI coverage for lobby behavior and relay framing/rate limits.

### Still required before cross-platform play
1. Add the WebGL 2 browser renderer and an Emscripten build of the game runtime.
2. Add browser-side disc-image selection and local-only loading; no disc image is uploaded to the server.
3. Adapt the game's lockstep protocol to the CBR1 WebSocket relay.
4. Add TLS/WSS to the PS3 client, then wire the guest-ticket relay client into the upstream net adapter with per-peer routing and frame batching.
5. Connect the PS3 in-game host/join UI to guest room codes.
6. Test matching builds, input delay, disconnect/rejoin, and deterministic frame fingerprints on a real PS3 and a real browser.

**Status:** the room relay is a transport milestone only. The browser game runtime and PS3-to-browser gameplay synchronization are not yet complete, so this branch must not be described as cross-platform playable yet.
