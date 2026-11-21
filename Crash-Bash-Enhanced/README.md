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
- Ready state and host-controlled match start.
- Host transfer if the host leaves.
- Authenticated host UDP endpoint publishing and peer discovery.
- Prototype room WebSocket relay with authenticated sender envelopes, 1200-byte payload limit, and per-client rate limiting.
- Responsive web lobby.
- CI tests for the Worker and web build.

## Important: gameplay status

The current milestone is **transport prototype, not playable cross-platform**. The lobby and binary WebSocket relay are implemented, but neither the PS3 PRX nor a browser game runtime is connected to that relay. The browser runtime still needs a WebGL 2 renderer and Emscripten build; the PS3 client still needs a TLS WebSocket transport and in-game room-code UI. Until both clients exchange synchronized game inputs in a real match, this project is not an online-playable release.

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

The target is Cloudflare Workers Free with SQLite-backed Durable Objects. Free usage is capped: the prototype currently limits each relay client to five incoming binary messages per second. The exact free-tier budget and the resulting four-player ceiling are documented in docs/CROSSPLAY-ARCHITECTURE.md. No home PC or PSN account is required.

## Automated checks

GitHub Actions builds the Vite lobby and runs the matchmaking Worker integration suite plus a Cloudflare dry-run bundle check on changes under this project directory.

## Release gates

1. Worker and web CI pass.
2. Deploy the Worker and verify the public health endpoint.
3. Configure the web API URL and publish the lobby.
4. Build the PS3 client with the correct PS3 SDK.
5. Verify two-console controller synchronization in Crash Bash.
6. Test disconnect/rejoin, latency, host loss, and repeated matches.
7. Only then label the project online-playable.
