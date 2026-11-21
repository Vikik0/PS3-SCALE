# Crash Bash Online matchmaking service

This Worker provides the first online-service layer for the Crash Bash Enhanced project.

## Included now

- Guest sessions; no PSN account or PSN login is requested.
- Private room codes and up to four player slots.
- Host handoff when the host leaves.
- Ready state and host-controlled match start.
- Room polling and peer roster endpoint.
- SQLite-backed Durable Object storage.

## Not included yet

This service is matchmaking only. It does not yet transmit controller inputs or synchronize the PS1 game. The PS3-side integration and frame/input synchronization must be implemented and tested separately before the game can be described as playable online.

## Free-tier note

Cloudflare Workers Free supports SQLite-backed Durable Objects with usage limits. The service is designed for lightweight room coordination, not for relaying game video or gameplay packets. Review current limits before public launch.

## Local checks

```bash
npm install
npm test
npm run check
npm run dev
```

## Deployment

Set `CLOUDFLARE_API_TOKEN` and `CLOUDFLARE_ACCOUNT_ID` as GitHub Actions secrets to enable the deployment workflow. No local PS3 or home PC server is required.
