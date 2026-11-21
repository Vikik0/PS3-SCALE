
# Feasibility — Call of Duty: Ghosts PS3 local Campaign co-op

## Current verdict

**Local two-player Campaign co-op is an unproven but better-scoped first milestone.** Ghosts has existing offline local-play modes reported to support two players, which gives us a native local-player path to investigate. The Campaign itself remains single-player, so the challenge is adapting Campaign initialization and scripts to accept the second local player without breaking story logic.

Online networking is not the current blocker because it is deferred until the local mode works.

## Initial target

- PS3 CEX CFW 4.93.
- Ghosts Title Update 1.16.
- Two local controllers on one console.
- Player 1 remains Logan.
- Player 2 is a separate Multiplayer-style soldier actor.
- Hesh and all story NPCs remain untouched.

## Public research findings

1. PS3 community reports list Multiplayer, Squads and Extinction as offline local modes with one or two players. Treat this as a lead to verify on the exact PS3 build, not as proof of Campaign support.
2. The original Campaign is single-player. No public source located in this research demonstrates an existing two-player Campaign mode.
3. Ghosts uses the IW6 engine. GSC Tool documents IW6 script compile/decompile support for PS3.
4. Public PS3 IW6 FastFile research documents the retail container as signed. It can be decompressed and inspected, but cannot be rebuilt/re-signed without the publisher's private key. GSC compilation alone does not solve this.
5. Public TU 1.16 offset lists contain entity/player script routines, but the list is multiplayer-context research and does not prove Campaign compatibility.
6. Ghosts modding guides describe SPRX/modified-EBOOT/RTM workflows and warn that CEX/DEX executable variants must not be mixed.

## Preferred technical strategy

Reuse the game's existing two-local-player initialization path from an offline mode. Compare the working local two-player mode with Campaign to identify which local-player structures, input routing, viewports, player entities and HUD state are absent in Campaign.

Only if the native path cannot be reused should we consider a lower-level second actor implementation. A spawned model/entity is not automatically a controllable player.

Because the retail FastFiles are signed, the project must not depend on an unverified extract-edit-repack workflow. Investigate safe runtime hooks in the exact user-owned executable and PRX environment instead.

## Main risks

- Campaign code may hard-code a single local player.
- Story triggers, cinematics, camera transitions and checkpoints may assume Logan is the only player.
- AI may target only the original Campaign player.
- The local split-screen setup from Multiplayer/Extinction may not be compatible with Campaign initialization.
- Candidate offsets may differ by Title ID, update, executable and CEX/DEX profile.
- An incorrect hook can freeze the game or corrupt mission state.

## Phase 1 proof gate

A one-mission local proof of concept must demonstrate:
1. Two local player slots initialize on one PS3 without network access.
2. Logan remains the original Campaign player and story progression owner.
3. Player 2 is a separate soldier controlled by controller 2.
4. Camera, input, HUD, weapons and health are independent.
5. AI can engage both players and core mission triggers continue to work.
6. One player can be downed while the other continues and revives them.
7. Checkpoint reload and mission restart restore both players correctly.
8. A 20-minute test completes without crash or softlock.

Only after this gate should the project expand to all Campaign missions.

## Phase 2 gate

Online networking begins only after Phase 1 is proven on real hardware. The current lobby, Worker, UDP/STUN and WebSocket components remain parked and are not part of the local prototype.
