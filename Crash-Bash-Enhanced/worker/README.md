# Crash Bash Online matchmaking service

This Worker provides the first online-service layer for the Crash Bash Enhanced project.

## Included now

- Guest sessions; no PSN account or PSN login is requested.
- PS3 room-code join endpoint that issues a short-lived, single-use WebSocket ticket instead of exposing a long-lived guest token to the PS3 client; a successful PS3 join is marked ready automatically.
- PS3 host-room creation endpoint with a private room code and one-time WebSocket ticket.
- Authenticated `START` WebSocket control; only the room host can activate a waiting match after all players are ready.
- Private and public room codes with up to four player slots.
- Read-only public room discovery at `GET /v1/rooms/public`; it exposes only waiting, non-full public rooms and does not require a guest session.
- Host handoff when the host leaves.
- Disconnect cleanup and idempotent browser rejoin.
- Ready state and host-controlled match start.
- Authenticated WebSocket room relay for opaque binary packets, with sender IDs, packet-size/rate limits, keepalive handling, and pushed room-state updates.
- Room state is pushed over the WebSocket relay; the browser does not poll every few seconds.
- Room snapshot and peer roster endpoints for recovery.
- Prototype binary WebSocket room relay at /v1/rooms/:code/transport.
- Authenticated CBR1 sender envelope, 1200-byte per-datagram cap, CBU1 batches of up to 32 targeted datagrams (24,576 bytes total), and a five-inbound-WebSocket-message-per-second cap per connected player. The cap includes text controls and keepalives.
- SQLite-backed Durable Object storage with WebSocket hibernation.

## Not included yet

The relay is a transport prototype. The PS3 adapter now intercepts the upstream lockstep UDP socket calls and routes CBU1/CBR1 packets with per-peer addressing and a 220 ms batch cadence. Host-side tests cover ticket flows, roster mapping, start control, batching, and envelope decoding. A future browser WebAssembly game runtime is not connected yet. The PS3 client now has certificate-verified TLS 1.2/WSS, but the public Worker deployment is still waiting for Cloudflare account credentials. Do not describe the game as playable online until the browser runtime and real PS3/browser match tests pass.

## Free-tier note

Cloudflare Workers Free supports SQLite-backed Durable Objects with strict daily limits. The prototype caps usage at 16 total browser and PS3 sessions, four open rooms, one active match at a time, and five inbound WebSocket messages per second per client, including controls and keepalive pings. The PS3 adapter batches upstream packets every 220 ms and uses a 12-step input delay, but real-world latency and billing still need validation. This is not an unlimited relay. See ../docs/CROSSPLAY-ARCHITECTURE.md for the current quota calculation.

## Local checks

```bash
npm install
npm test
npm run check
npm run dev
```

## Deployment

The deployment workflow builds `../web` and uploads its static output with the Worker, so the lobby, HTTPS API, and WSS relay share one `workers.dev` origin. The production lobby uses same-origin requests when `VITE_API_BASE_URL` is blank. Set `CLOUDFLARE_API_TOKEN` and `CLOUDFLARE_ACCOUNT_ID` as GitHub Actions secrets to enable public deployment. No local PS3 or home PC server is required.
