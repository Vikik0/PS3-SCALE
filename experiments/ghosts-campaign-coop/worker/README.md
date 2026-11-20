# Free matchmaking / P2P signalling deployment

## Design

This Cloudflare Worker uses one SQLite-backed Durable Object as the lobby state store. It handles guest sessions, six-character room codes, the two-player limit, ready/start state, and exchange of STUN-derived UDP candidates.

Gameplay packets are not sent through Cloudflare. Once candidates are exchanged, the PS3 clients attempt direct authenticated UDP peer-to-peer traffic using the GHC2 packet codec. This keeps the free-tier service limited to matchmaking and connection setup.

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

The candidate endpoint validates the advertised public IP against Cloudflare's CF-Connecting-IP header when deployed behind Cloudflare.

## Local validation

1. Run npm install in this folder.
2. Run npm run check to bundle and validate the Worker.
3. Run npm run dev to start the local Durable Object emulator.

## Deploy

The repository includes a GitHub Actions deployment workflow. It runs only when both repository secrets exist:

- CLOUDFLARE_API_TOKEN: token with permission to deploy Workers and manage Durable Objects.
- CLOUDFLARE_ACCOUNT_ID: the target Cloudflare account ID.

The Worker can be deployed to the free Workers plan for low-volume matchmaking/signalling. This is not a UDP relay and does not provide a TURN fallback.

## Network limitation

STUN plus UDP hole punching works on many home NAT configurations but cannot guarantee connectivity through symmetric NAT, carrier-grade NAT, or networks that block peer UDP. Those cases need a TURN/relay service, which is not included because a general-purpose game relay is not reliably free. The client must report a clear NAT/connectivity failure instead of pretending the room is connected.

## Security

Session tokens are random 256-bit values; only their SHA-256 hashes are stored. The relay key is per-session and is shared only with the paired player over HTTPS. The Worker never receives gameplay packets.
