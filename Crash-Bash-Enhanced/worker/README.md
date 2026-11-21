# Crash Bash Online matchmaking service

This Worker provides the first online-service layer for the Crash Bash Enhanced project.

## Included now

- Guest sessions; no PSN account or PSN login is requested.
- PS3 room-code join endpoint that issues a short-lived, single-use WebSocket ticket instead of exposing a long-lived guest token to the PS3 client.
- Private room codes and up to four player slots.
- Host handoff when the host leaves.
- Disconnect cleanup and idempotent browser rejoin.
- Ready state and host-controlled match start.
- Authenticated WebSocket room relay for opaque binary packets, with sender IDs, packet-size/rate limits, keepalive handling, and pushed room-state updates.
- Room state is pushed over the WebSocket relay; the browser does not poll every few seconds.
- Room snapshot and peer roster endpoints for recovery.
- Prototype binary WebSocket room relay at /v1/rooms/:code/transport.
- Authenticated CBR1 sender envelope, 1200-byte payload cap, and five inbound messages/second per connected player, including keepalive pings.
- SQLite-backed Durable Object storage with WebSocket hibernation.

## Not included yet

The relay is a transport prototype only. The PS3 UDP lockstep adapter and a future browser WebAssembly game runtime are not yet connected to it. The CBR1 envelope is documented in ../docs/CROSSPLAY-ARCHITECTURE.md. Do not describe the game as playable online until the PS3 and browser clients exchange real lockstep packets and pass hardware/browser match tests.

## Free-tier note

Cloudflare Workers Free supports SQLite-backed Durable Objects with strict daily limits. The prototype caps usage at 16 guest sessions, four open rooms, one active match at a time, and five inbound WebSocket messages per second per client (including keepalive pings). This is not an unlimited relay. See ../docs/CROSSPLAY-ARCHITECTURE.md for the current quota calculation.

## Local checks

```bash
npm install
npm test
npm run check
npm run dev
```

## Deployment

Set `CLOUDFLARE_API_TOKEN` and `CLOUDFLARE_ACCOUNT_ID` as GitHub Actions secrets to enable the deployment workflow. No local PS3 or home PC server is required.
