# Crash Bash Online matchmaking service

This Worker provides the first online-service layer for the Crash Bash Enhanced project.

## Included now

- Guest sessions; no PSN account or PSN login is requested.
- Private room codes and up to four player slots.
- Host handoff when the host leaves.
- Ready state and host-controlled match start.
- Room state is pushed over the WebSocket relay; the browser does not poll every few seconds.
- Room polling and peer roster endpoint.
- Prototype binary WebSocket room relay at /v1/rooms/:code/transport.
- Authenticated CBR1 sender envelope, 1200-byte payload cap, and five inbound messages/second per connected player, including keepalive pings.
- SQLite-backed Durable Object storage with WebSocket hibernation.

## Not included yet

The relay is a transport prototype only. The PS3 UDP lockstep adapter and a future browser WebAssembly game runtime are not yet connected to it. The CBR1 envelope is documented in ../docs/CROSSPLAY-ARCHITECTURE.md. Do not describe the game as playable online until the PS3 and browser clients exchange real lockstep packets and pass hardware/browser match tests.

## Free-tier note

Cloudflare Workers Free supports SQLite-backed Durable Objects with strict daily limits. The prototype rate limit is deliberately low to keep a small four-player room within the free-tier budget; it is not an unlimited relay. See ../docs/CROSSPLAY-ARCHITECTURE.md for the current quota calculation.

## Local checks

```bash
npm install
npm test
npm run check
npm run dev
```

## Deployment

Set `CLOUDFLARE_API_TOKEN` and `CLOUDFLARE_ACCOUNT_ID` as GitHub Actions secrets to enable the deployment workflow. No local PS3 or home PC server is required.
