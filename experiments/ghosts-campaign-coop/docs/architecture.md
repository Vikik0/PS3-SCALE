# Architecture

## Target experience

- Two players maximum.
- Player 1 remains Logan and drives the original Campaign story flow.
- Player 2 is a separate Multiplayer-style soldier actor; story NPCs such as Hesh remain untouched.
- Both players share mission state, combat encounters and checkpoints.
- PSN and Guest access are separate connection modes. Guest mode uses the private service and does not require PSN. This prototype does not bypass PSN bans or authenticate PSN.

## Components

1. **PS3 client plugin (not implemented):** game-specific hooks plus HTTP session/lobby calls and UDP relay client.
2. **Lobby API (implemented prototype):** guest session creation, six-character room codes, join/leave, ready state, two-player cap and host start.
3. **UDP relay (implemented reference prototype):** forwards authenticated opaque packets only between the two members of a running room. The Python reference relay is locally integration-tested; it is not the Cloudflare deployment.
4. **Cloudflare WebSocket relay fallback (implemented in Worker source):** an authenticated WSS route forwards binary GHC2 frames between the two room members when direct UDP cannot connect. It checks HMAC, replay window, frame size and per-session rate. Deployment and PS3 client integration are still pending.
4. **Compatibility registry (implemented):** maps known title IDs and reports the first target profile (CEX 4.93 + TU 1.16). It intentionally does not report a profile as supported until hardware validation.
5. **Campaign downed/revive rules (implemented as a standalone C state machine):** one player can be downed while the mission continues; the living partner can revive them; both downed or bleedout expiry fails the mission. A versioned 24-byte snapshot is ready to travel inside GHC2 payloads. The game-specific hooks are not implemented yet.
6. **Game replication layer (not implemented):** movement, animation, weapon, damage, AI, checkpoint and mission-event replication.
7. **PSN identity adapter (not implemented):** requires a legitimate supported platform identity mechanism. No fake PSN tokens or ban evasion are provided.

## Authority model to test

Initially Player 1/Logan's console is the mission authority. Player 2 sends input/state; host sends authoritative mission events and AI state. This is only a starting design. It must be validated against Ghosts' actual Campaign code and can be changed if the engine's existing networking model requires a different authority model.

## Network envelope

The authenticated GHC2 envelope is shared by the direct UDP path and the WebSocket relay fallback:

Client -> relay:
`GHC2 | 16-byte relay ID | uint32 sequence (big-endian) | payload | 16-byte HMAC-SHA256 tag`

Relay -> peer:
`GHC2 | uint8 sender slot | uint32 sequence (big-endian) | payload | 16-byte HMAC-SHA256 tag`

The bearer session token is used only in HTTPS/WSS Authorization headers. The relay key is issued by the HTTPS guest-session endpoint. The relay validates the HMAC, rejects duplicate/stale sequence numbers with a wrap-safe 64-packet window, caps frames at 1200 bytes and rate-limits each session. The gameplay payload remains opaque until game-state fields are reverse-engineered and validated.

If direct UDP hole-punching fails, both PS3 clients can use the WSS relay path `/v1/rooms/{code}/relay`. The Worker forwards binary GHC2 frames only; the PS3 WSS client is not implemented yet.

## Prototype limitations

- In-memory rooms; server restart clears all rooms.
- Guest sessions expire after 12 hours.
- No NAT traversal, relay region selection, persistence, encryption, DDoS protection or production deployment configuration yet.
- UDP relay is a transport experiment, not a complete netcode implementation.
- PSN mode is explicitly unconfigured.
- Title ID registry is metadata only; no EBOOT/SPRX is built.


## Downed / revive flow

The host is authoritative for the mission rules:

1. Player health reaches the Campaign downed threshold; that player enters DOWNED.
2. The other player remains active and may keep fighting.
3. The living player holds the revive input while in range and with line of sight.
4. Releasing the input or breaking range/line-of-sight cancels progress.
5. A completed revive returns the downed player at the configured revive health.
6. If both players are down, the host marks the mission failed immediately.
7. If the bleedout timer expires before revival, the host marks the mission failed.
8. The Campaign adapter must call the real checkpoint restart path once and then replicate the checkpoint reset.

Initial tuning: 30-second bleedout, 5-second hold-to-revive, 40% health on revive. These are project defaults, not claimed exact Extinction timings. The downed crawl/sidearm behavior and all animation/HUD hooks remain dependent on Campaign integration.
