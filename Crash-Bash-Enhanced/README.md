# Crash Bash Enhanced

Crash Bash Enhanced is a community project to add a modern online layer around the original four-player party game.

## Project structure

- `web/`: responsive room/lobby web interface, built with Vite.
- `worker/`: Cloudflare Worker + SQLite-backed Durable Object for guest sessions and room matchmaking.
- `ps3/`: reserved for the native PS3 / PS1-emulator integration and controller-input synchronization.

## Current implementation

- Guest sessions with no PSN login.
- Private six-character room codes.
- Two-to-four player lobbies.
- Ready state and host-controlled match start.
- Host transfer if the host leaves.
- Responsive web lobby.
- CI tests for the Worker and web build.

## Important: gameplay status

The current milestone is **matchmaking only**. It does not yet send controller inputs between PS3 consoles or synchronize Crash Bash gameplay. The PS3-side work must identify the exact Crash Bash executable / PS1 emulator path on the target CFW setup, then implement and test frame/input synchronization. Until that works on two real consoles, this project is not an online-playable release.

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

The Worker is designed for Cloudflare Workers Free with SQLite-backed Durable Objects. Free-plan limits apply; the service is designed for small lobby/signalling messages, not gameplay packet relaying. No home PC or PSN account is required for matchmaking.

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
