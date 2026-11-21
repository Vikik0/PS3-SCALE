
# Local Campaign co-op — Phase 1 plan

## Goal

Add a two-player offline Campaign mode to the PS3 version of Call of Duty: Ghosts:
- one PS3;
- two DualShock 3 controllers;
- no internet, PSN, lobby or server;
- Player 1 remains Logan;
- Player 2 is a separate Multiplayer-style soldier;
- story NPCs are never reused as the second player.

Online co-op is Phase 2 and does not begin until the local proof of concept works.

## Why the local engine path matters

Ghosts already has offline local-play modes reported to support two players. The most valuable starting point is the native second-player setup used by those modes: local player slots, controller assignment, player state, camera/viewports and HUD. Reusing that machinery is more promising than spawning a generic soldier entity and trying to turn it into a player.

The PS3 GameFAQs Q&A and MobbyGames technical entry are community/database references, not official proof of every mode on every region. The exact local modes must be confirmed against the target PS3 build.

## Research completed

### Game update identity

The public PS3 update catalogue lists Ghosts Title Update 1.16 for the US and EU retail/digital Title IDs. 1.16 is the final update identified by the community modding research. The exact package must match the game Title ID; one region's update is not interchangeable with another.

### Script/FastFile pipeline

- GSC Tool supports IW6 / Ghosts script compile and decompile for PS3.
- Public PS3 IW6 research documents signed retail FastFiles. They can be decompressed and inspected, but cannot be rebuilt/re-signed without the publisher's private key.
- Therefore, compiling a modified GSC file is not sufficient to install it in the retail game. Do not assume the old extract-edit-repack patch_mp.ff workflow works.
- The current likely path is carefully verified EBOOT/PRX runtime hooks or another proven runtime script-loading mechanism.

### Spawn functions

A public TU 1.16 offsets thread lists G_Spawn, G_CallSpawnEntity, PlayerCMD_Spawn and PlayerCMD_ClonePlayer. These addresses are Multiplayer-context leads only. No candidate is approved for Campaign until verified against the exact executable and calling convention.

## Work sequence

1. **Baseline local mode:** establish which Ghosts offline mode initializes two local players on the exact PS3 release. Record controller mapping, player slots, camera/viewports, HUD and restart behavior.
2. **Campaign comparison:** compare one-player Campaign runtime state with the two-player local-mode state. Identify which player/client initialization and split-screen components are missing.
3. **Campaign second slot:** find a safe way to retain or initialize local player slot 2 during Campaign startup.
4. **Player identity:** keep Logan as Player 1 and assign a separate soldier actor to Player 2. Do not substitute a story NPC.
5. **Independent controls:** verify each DualShock controls only its own player, camera, weapon and HUD.
6. **Campaign compatibility:** verify AI targeting, objective triggers, vehicles, cinematics, scripted sequences and checkpoints.
7. **Revive:** connect the existing downed/revive rules to Campaign health, animation and checkpoint failure logic.
8. **Mission coverage:** expand from one mission to all Campaign missions and edge cases.
9. **Online conversion:** only after the local build passes the hardware gate, add remote transport and state synchronization.

## Phase 1 success gate

A single Campaign mission must run for at least 20 minutes with:
- two independent local players;
- no network dependency;
- Logan and the second soldier both controllable;
- independent camera and HUD;
- enemies able to engage both;
- objectives and story triggers functioning;
- downed/revive functioning;
- checkpoint reload and restart functioning;
- no crash or softlock.

## Explicitly not done

No Campaign second-player slot, local input hook, split-screen camera hook or mission adapter is implemented yet. The current PRX is a passive build scaffold. This plan does not claim a playable mod exists.
