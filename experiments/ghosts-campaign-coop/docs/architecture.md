
# Architecture — local-first Campaign co-op

## Target experience

- Two players on one PS3, using two DualShock 3 controllers.
- No internet, PSN, lobby or external server in Phase 1.
- Player 1 remains Logan and owns the original Campaign story progression.
- Player 2 is a separate Multiplayer-style soldier/player actor.
- Hesh and all other story NPCs remain untouched.
- Both players share mission objectives, combat encounters and checkpoints.

## Prior art: Co-op Anything / Modern Warfare 2 Enhanced

The research in [docs/coop-anything-prior-art.md](coop-anything-prior-art.md) reviews the Co-op Anything playbook and the MW2 Enhanced IW-engine reference. It supports our decision to reuse Ghosts' native local-player machinery first, while treating MW2/PS4/XB3 code and offsets as non-transferable research only.

## Phase 1 architecture: one console, two local players

### 1. Reuse the engine's local-player system first

Ghosts already has offline local-play modes reported to support two players. The first reverse-engineering task is to identify how those modes initialize the second local player:
- local player/client count and slot allocation;
- controller-to-local-player mapping;
- player entity/player-state initialization;
- split-screen viewport and camera setup;
- per-player HUD, weapon and input state;
- teardown and checkpoint/restart behavior.

The preferred path is to reuse this native local-player machinery. Do not begin by cloning a random entity or by adding a custom controller polling layer. A generic spawned entity is not equivalent to a fully initialized local player.

### 2. Campaign bootstrap

Compare a known-working two-player offline mode with a one-player Campaign session. Identify the minimum state and initialization differences needed to retain local player slot 1 while loading a Campaign mission.

The Campaign adapter must preserve Logan's original story flow. Player 2 should occupy the second local player slot and use a separate soldier actor. The implementation must not repurpose Hesh, Elias, Merrick or another scripted NPC.

### 3. Camera, input and HUD

Use the game's own local-player viewport/input paths wherever possible. Verify:
- controller 1 controls only Logan;
- controller 2 controls only Player 2;
- each player has an independent view/camera;
- aim, fire, reload, movement and weapon switching remain independent;
- HUD elements and prompts are associated with the correct local player;
- scripted camera sequences do not steal or merge both players' input.

### 4. Campaign systems

Campaign code may assume one player for triggers, objective ownership, cinematics, camera transitions, checkpoints, AI targeting, vehicles, scripted sequences, health and mission state. Each assumption must be tested and adapted only where required.

### 5. Downed/revive

The existing C state machine is a standalone rules component. In local mode one console is authoritative:
1. One player becomes downed; the other remains active.
2. The active player can continue combat and revive the downed player.
3. Releasing the revive input, leaving range or losing line of sight cancels progress.
4. Both players down or bleedout expiry triggers the Campaign's restart/fail path.
5. Checkpoint reset must restore both local player slots without duplicating or losing story actors.

Initial tuning remains 30-second bleedout, 5-second hold-to-revive and 40% revive health. These are project defaults, not verified game timings.

## Phase 1 implementation gates

1. **Baseline:** verify two-controller local play in an existing Ghosts local mode on the PS3 target.
2. **Initialization:** identify and document the second local-player/client initialization path and relevant state structures.
3. **Campaign entry:** start one selected Campaign mission with two local slots without modifying Logan's story identity.
4. **Control isolation:** both controllers independently control their own player and camera.
5. **Gameplay:** movement, weapons, damage, AI targeting and basic objective triggers work for both players.
6. **Recovery:** downed/revive, checkpoint reload and mission restart work.
7. **Stability:** complete a 20-minute test with no crash, softlock or broken story trigger.
8. **Coverage:** expand one-mission proof to the remaining Campaign missions, cinematics and scripted set pieces.

Do not call Phase 1 complete until the above gates pass on real hardware.

## Reverse-engineering constraints

### Signed FastFiles

The public PS3 IW6 research describes Ghosts FastFiles as signed containers that can be decompressed and inspected, but not rebuilt/re-signed without Infinity Ward's private signing key. Therefore, do not assume that editing an extracted GSC and repacking the retail FastFile will work.

GSC Tool supports IW6 PS3 script compile/decompile operations, but that only handles the script bytecode format. It does not grant the ability to produce a valid signed retail FastFile. The likely research path is executable/PRX runtime hooks or another verified runtime-loading mechanism, not a casual patch_mp.ff replacement.

### Candidate offsets

The public TU 1.16 offset list is multiplayer-oriented. Addresses for G_Spawn, G_CallSpawnEntity, PlayerCMD_Spawn and PlayerCMD_ClonePlayer are research leads only. Their Campaign safety, call signatures, local-client behavior and CEX 4.93 mapping are unknown. Never call them blindly.

## Phase 2 architecture: online (deferred)

Only after local co-op is stable:
- connect the two local-player state streams to a remote peer;
- choose host authority and conflict resolution;
- reuse GCP1 message contracts where suitable;
- enable the existing GHC2 authentication and replay protection;
- integrate direct UDP/STUN and WebSocket relay fallback;
- add remote session/lobby handling.

The current lobby, Worker, STUN, UDP and WebSocket source is parked. None of it is required to prove local Campaign co-op.

## Current limitations

- No Campaign player-two spawn hook is implemented.
- No local second-player initialization hook is implemented.
- No independent Campaign camera/HUD setup is implemented.
- No Campaign movement, damage, AI or checkpoint replication is connected.
- No live PS3 hardware test has been performed.
- The current PRX is a passive bootstrap only.
