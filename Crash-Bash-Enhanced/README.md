# Crash Bash Enhanced

Crash Bash Enhanced is a community project to add a modern online layer around the original four-player party game.

## Project structure

- `web/`: responsive room/lobby web interface, built with Vite.
- `worker/`: Cloudflare Worker + SQLite-backed Durable Object for guest sessions, room matchmaking, and a prototype binary WebSocket relay.
- `ps3/`: native PS3 / PS1-emulator input bridge and the current UDP lockstep adapter.
- `docs/CROSSPLAY-ARCHITECTURE.md`: selected browser/PS3 architecture, relay wire format, free-tier budget, and remaining release gates.

## Current implementation

- Guest sessions with no PSN login.
- Private six-character room codes.
- Two-to-four player lobbies.
- Free-tier safety caps: 16 guest sessions, four open rooms, and one active match at a time.
- Ready state and host-controlled match start.
- Host transfer if the host leaves.
- Authenticated host UDP endpoint publishing and peer discovery.
- Prototype room WebSocket relay with authenticated sender envelopes, 1200-byte payload limit, and per-client rate limiting.
- PS3 relay client with TLS 1.2, WSS, SNI, and trusted-CA certificate verification.
- Host integration tests for both plain WebSocket and verified local HTTPS/WSS connections.
- Push-based room updates over WebSocket; no rapid lobby polling.
- Responsive web lobby.
- CI tests for the Worker and web build.

## Important: gameplay status

The current milestone is **transport prototype, not playable cross-platform**. The PS3 PRX now has a certificate-verified TLS 1.2/WSS transport and an in-game room-code menu, but a real Worker endpoint still needs deployment credentials and the PRX has not been tested on physical PS3 hardware. The browser lobby is not yet a game runtime: it still needs a WebGL 2 renderer and Emscripten build. Until both clients exchange synchronized game inputs in a real match, this project is not an online-playable release.

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

GitHub Actions builds the Vite lobby, runs Worker integration tests, validates the Cloudflare bundle, tests WS/WSS with certificate verification against local TLS servers, and cross-compiles the PS3 PRX. The deployment workflow still skips deployment until Cloudflare credentials are configured.

## Release gates

1. Keep Worker, web, WS/WSS, and PS3 cross-build CI green.
2. Configure Cloudflare Actions secrets and deploy the Worker; verify the public health endpoint.
3. Configure the web API URL and publish the lobby.
4. Build an installable PS3 PKG around the PRX and loader.
5. Verify the plugin loads and the room menu works on physical PS3 hardware.
6. Complete the browser WebGL 2/Emscripten game runtime.
7. Verify a real PS3-to-browser match, then test disconnect/rejoin, latency, host loss, and repeated matches.
