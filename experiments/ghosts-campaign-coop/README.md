
# Ghosts Campaign Co-op — Local-first Prototype 0.3

An isolated research experiment to add two-player co-op to the original PS3 Campaign of Call of Duty: Ghosts.

## Agreed development order

1. **Phase 1 — Local Campaign co-op:** two controllers on one PS3, offline, no PSN, no lobby and no internet connection.
2. **Phase 2 — Online co-op:** only after the local Campaign build is proven playable. Reuse the game-state contract and parked network modules where practical.

Do not spend Phase 1 effort deploying online services or implementing NAT traversal.

## Phase 1 target

- Console: PS3 CEX CFW 4.93 (initial development target only; not hardware-verified).
- Game update: Title Update 1.16.
- Maximum players: 2 local players.
- Player 1 remains Logan and preserves the original Campaign flow.
- Player 2 is a separate Multiplayer-style soldier/player actor, not Hesh or another story NPC.
- Each player uses a separate DualShock 3 controller.
- The game runs offline on one console.

## Research findings

- PS3 patch notes explicitly mention a split-screen UI fix, and community reports describe two-player offline local play in Multiplayer, Squads and Extinction. This is a lead for reusing Ghosts' existing local-player initialization; it is not proof that Campaign can accept a second player.
- The original Campaign is documented as single-player. Adding a second Campaign participant requires engine/gameplay integration.
- Ghosts PS3 IW6 FastFiles can be decompressed and inspected, but the documented retail container is signed and cannot be rebuilt/re-signed without the publisher's private signing key. A simple extract-edit-repack GSC workflow is therefore not a viable assumption.
- GSC Tool supports IW6 scripts on PS3 for compile/decompile work. This does not itself solve signed FastFile repackaging.
- Public TU 1.16 spawn offsets are multiplayer-context leads only. They are not approved Campaign addresses.

See docs/local-coop-first.md for the implementation plan and research constraints. The cross-project IW-engine prior-art audit is in docs/coop-anything-prior-art.md.

## Current implementation status

### Implemented as standalone components (not yet connected to Campaign)

- Downed/revive state machine and portable tests.
- GHC2 authenticated packet codec and replay-window tests.
- GCP1 versioned gameplay-message codec and tests.
- UDP/STUN helpers and direct-P2P packet layer.
- Lobby/Worker prototype and WebSocket frame codec.
- PS3 PRX bootstrap build and CI validation.
- Host-tested DualShock 3 input decoder, PS3 `sys_io` polling adapter for controller ports 0 and 1, and portable per-frame input tracker (button edges, normalized sticks and read-error preservation).
- Diagnostic PRX polling/getter functions for raw and per-frame input, pending a verified Campaign frame adapter.
- Reverse-engineering notes rule out treating `PlayerCMD_ClonePlayer` as a full second-player spawn path.

### Phase 1 Campaign integration — not implemented

- Discover and verify the game's existing local second-player initialization path.
- Keep two local player slots active while starting a Campaign mission.
- Bind controller 1 to Logan and controller 2 to the new player slot.
- Establish independent camera/view/HUD state.
- Create or reuse the second soldier actor without replacing story NPCs.
- Connect Campaign movement, weapons, damage, AI targeting, mission triggers and checkpoints.
- Connect downed/revive rules to real Campaign health and animations.
- Test mission transitions and all Campaign-specific one-player assumptions.

The PRX still does not spawn a player or patch Campaign state. It includes an unhooked controller-input diagnostic adapter; no live Campaign frame hook or second-player runtime integration exists. No build is currently playable.

## Phase 2 — parked until Phase 1 passes

The following work remains in source but is intentionally deferred:
- Guest lobby and room codes.
- Cloudflare Durable Object Worker.
- HTTPS lobby client.
- STUN and UDP hole punching.
- WebSocket relay fallback.
- Network replication and PSN identity research.

## Compatibility policy

The first target is CEX 4.93 + TU 1.16. The project must select builds by exact game Title ID, update and firmware family. Do not install a DEX EBOOT on CEX or mix regional executable profiles. Known regional IDs are listed in docs/compatibility-matrix.md; none is hardware-validated for this mod yet.

## Verification

CI validates the standalone codecs, controller-input decoder, and PRX build including `sys_io` imports. These tests do not prove that the game runtime can use a second player in Campaign. The Phase 1 success gate is defined in docs/local-coop-first.md.

## Important

No retail game files, copyrighted game assets, original EBOOT or installable game PKG are included. The project uses public technical research and will require exact executable identity and runtime validation before enabling any game-memory hooks.
