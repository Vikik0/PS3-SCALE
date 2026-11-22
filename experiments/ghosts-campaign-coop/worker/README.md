# Free matchmaking, P2P signalling and WebSocket relay fallback

## Design

This Cloudflare Worker uses a SQLite-backed Durable Object for guest sessions, six-character room codes, the two-player limit, ready/start state and peer candidate exchange.

Gameplay transport has two paths:

1. **Direct UDP:** clients exchange STUN-derived candidates and attempt authenticated GHC2 UDP peer-to-peer traffic.
2. **WebSocket fallback:** if direct UDP cannot connect, both clients can open the authenticated secure WebSocket endpoint `/v1/rooms/{code}/relay`. The Durable Object forwards binary GHC2 packets only between the two members of the running room.

The WebSocket fallback validates the sender's relay ID and truncated HMAC-SHA256 tag, enforces a wrap-safe 64-packet replay window and a 120-packet/second per-session limit, caps frames at 1200 bytes, and re-signs the forwarded packet with the recipient's relay key. Session bearer tokens are sent only in the HTTPS/WSS Authorization header and are never placed in URLs.

## Endpoints

- GET /health
- GET /v1/compatibility
- POST /v1/sessions
- POST /v1/rooms
- POST /v1/rooms/{code}/join
- GET /v1/rooms/{code}
- POST /v1/rooms/{code}/ready
- POST /v1/rooms/{code}/start
- POST /v1/rooms/{code}/leave
- POST /v1/rooms/{code}/candidate
- GET /v1/rooms/{code}/peer
- **WSS /v1/rooms/{code}/relay** — authenticated binary gameplay relay fallback

## Local validation

1. Run `npm install` in this folder.
2. Run `npm test` to validate matchmaking, WebSocket pairing, GHC2 authentication, forwarding, replay rejection and disconnect cleanup.
3. Run `npm run check` to bundle and validate the Worker.
4. Run `npm run dev` to start the local Durable Object emulator.

## Deploy

The repository includes a GitHub Actions deployment workflow. It runs only when both repository secrets exist:

- `CLOUDFLARE_API_TOKEN`: token with permission to deploy Workers and manage Durable Objects.
- `CLOUDFLARE_ACCOUNT_ID`: the target Cloudflare account ID.

The Worker can be deployed to the Cloudflare free tier within its current usage limits. Production deployment is not confirmed until the workflow has credentials and completes successfully.

## Current limitations

- The PS3 client does not yet implement HTTPS guest-session creation, WebSocket upgrade, or live gameplay packet transport.
- The existing Python UDP relay is a local reference implementation; it is not the Cloudflare deployment.
- The Worker relay fallback is implemented and covered by its Node smoke tests, but it has not yet been deployed or tested from a PS3.
- PSN authentication is not configured. Guest mode is independent of PSN and does not bypass PSN bans.
- A successful lobby/relay connection does not prove that Campaign can spawn and control a second actor. Campaign actor hooks and gameplay replication remain unimplemented.

## Security

Session tokens are random 256-bit values; only their SHA-256 hashes are stored. The relay key is per-session and is shared with the paired player only through HTTPS lobby responses. The relay never accepts text frames or packets with an invalid HMAC, stale sequence, oversized frame or non-member session.
