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

Every client sends one binary game packet as the WebSocket message payload. The relay forwards it to the other connected players in the same room and adds an authenticated sender envelope:

| Offset | Bytes | Meaning |
| --- | ---: | --- |
| 0 | 4 | ASCII magic CBR1 |
| 4 | 16 | Sender session ID, decoded from the 32-character hex session ID |
| 20 | 2 | Payload length, unsigned big-endian |
| 22 | 1–1200 | Opaque game packet |

The server obtains the sender ID from the authenticated guest session, not from a client-supplied field. Text messages are rejected. The initial relay prototype caps each connected player at five inbound WebSocket messages per second (including keepalive pings) and 1200 payload bytes per message. The upstream lockstep protocol batches 32 game steps per packet at roughly 33.37 ms per step, so normal game traffic is expected to be around one packet per second per player; the five-message limit is a burst guard, not the intended send rate.

## Free-tier budget

The free plan is not unlimited. Cloudflare's current Durable Objects pricing lists 100,000 requests per day and applies a 20:1 billing ratio to incoming WebSocket messages. Four players each sending five packets per second would use approximately 86,400 metered request-equivalents per day, before room creation, connection upgrades, and other service traffic. This is an intentionally conservative prototype ceiling, not a promise of unlimited concurrent matches. The service must reject excess traffic rather than enable paid usage.

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
4. Replace or supplement the PS3 direct-UDP transport with a TLS WebSocket client compatible with the same relay.
5. Connect the PS3 in-game host/join UI to guest room codes.
6. Test matching builds, input delay, disconnect/rejoin, and deterministic frame fingerprints on a real PS3 and a real browser.

**Status:** the room relay is a transport milestone only. The browser game runtime and PS3-to-browser gameplay synchronization are not yet complete, so this branch must not be described as cross-platform playable yet.
