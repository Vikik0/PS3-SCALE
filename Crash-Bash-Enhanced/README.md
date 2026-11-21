# Crash Bash Enhanced

Crash Bash Enhanced is a community project to add a modern online layer around the original four-player party game.

## Project structure

- `web/`: responsive room/lobby web interface, built with Vite.
- `worker/`: Cloudflare Worker + SQLite-backed Durable Object for guest sessions, room matchmaking, and a prototype binary WebSocket relay. It also serves the built web lobby from the same origin.
- `ps3/`: native PS3 / PS1-emulator input bridge and the CBN1 lockstep adapter over WSS.
- `docs/CROSSPLAY-ARCHITECTURE.md`: selected browser/PS3 architecture, relay wire format, free-tier budget, and remaining release gates.

## Current implementation

- Guest sessions with no PSN login.
- Private and public six-character rooms, with a read-only public-room directory.
- Two-to-four player lobbies.
- Free-tier safety caps: 16 total browser and PS3 sessions, four open rooms, and one active match at a time.
- Ready state and host-controlled match start.
- Host transfer if the host leaves.
- Authenticated host UDP endpoint publishing and peer discovery.
- Prototype room WebSocket relay with authenticated sender envelopes, 1200-byte payload limit, and per-client rate limiting.
- PS3 relay client with TLS 1.2, WSS, SNI, and trusted-CA certificate verification.
- Installable PS3 homebrew PKG for the VSH loader and input bridge; CI validates the PKG identity and GNPDRM finalization.
- Host integration tests for both plain WebSocket and verified local HTTPS/WSS connections.
- Push-based room updates over WebSocket; no rapid lobby polling.
- Responsive web lobby with private-code joining and public-room discovery.
- CI tests for the Worker and web build.

## Important: gameplay status

The current milestone is **cross-platform protocol integrated in source, not yet hardware-verified**. The PS3 PRX has a certificate-verified TLS 1.2/WSS transport and an in-game room-code menu. The browser EmulatorJS player now joins PS3-hosted rooms, performs the CBN1 lobby/save/start handshake, captures local controller input, and advances the browser emulator from authoritative host records. CI covers the packet codec, browser lockstep flow, Worker relay, PS3 WSS client, PRX build, and PKG validation. The Worker and static site are not deployed because Cloudflare credentials are not configured, and neither the PRX nor a complete PS3/browser match has been tested on physical hardware.

The project will not use copyrighted game assets or distribute a game ISO/ROM. Players must supply their own legally obtained game copy.

## Local development

### Web lobby

```bash
cd web
cp .env.example .env
npm install
npm run dev
```

Set `VITE_API_BASE_URL` to the deployed Worker URL.

### Matchmaking Worker

```bash
cd worker
npm install
npm test
npm run check
npm run dev
```

## Free hosting target

The target is Cloudflare Workers Free with SQLite-backed Durable Objects. Free usage is capped: the prototype currently limits each relay client to five incoming WebSocket messages per second, including keepalive pings. The exact free-tier budget and the resulting four-player ceiling are documented in docs/CROSSPLAY-ARCHITECTURE.md. No home PC or PSN account is required.

## Automated checks

GitHub Actions builds the Vite lobby, runs Worker integration tests, validates the Cloudflare bundle, tests WS/WSS with certificate verification against local TLS servers, cross-compiles both PS3 PRXs, and builds and validates the installable PS3 PKG. The deployment workflow builds the web lobby and deploys it together with the Worker; the public deployment still waits for Cloudflare credentials.

## Release gates

1. Keep Worker, web, WS/WSS, and PS3 cross-build CI green.
2. Configure Cloudflare Actions secrets; the deployment workflow builds the lobby, deploys it with the Worker, and verifies the public health endpoint.
3. Install the generated PS3 PKG and verify the plugin loads and the room menu works on physical PS3 hardware.
4. Configure Cloudflare deployment credentials and deploy the Worker plus static lobby.
5. Install the generated PKG on physical PS3 hardware and verify loader injection, the in-game room menu, and the PS3 host handshake.
6. Run a real PS3-to-browser match with matching NTSC disc revisions; test input delay, packet loss, disconnect/rejoin, host loss, and repeated matches.
