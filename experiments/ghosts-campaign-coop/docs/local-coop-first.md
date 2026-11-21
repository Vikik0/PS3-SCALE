
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

The PS3 title-update notes include a fix for a split-screen UI issue, which confirms the PS3 game has split-screen UI behavior. A PS3 GameFAQs Q&A also reports local two-player Multiplayer, Squads and Extinction. These sources support investigating the native local-player path, but do not prove Campaign can use it. The exact local modes must be confirmed against the target PS3 build.

## Research completed

### Game update identity

The public PS3 update catalogue lists Ghosts Title Update 1.16 for the US and EU retail/digital Title IDs. 1.16 is the final update identified by the community modding research. The exact package must match the game Title ID; one region's update is not interchangeable with another.

### Script/FastFile pipeline

- GSC Tool supports IW6 / Ghosts script compile and decompile for PS3.
- Public PS3 IW6 research documents signed retail FastFiles. They can be decompressed and inspected, but cannot be rebuilt/re-signed without the publisher's private key.
- Therefore, compiling a modified GSC file is not sufficient to install it in the retail game. Do not assume the old extract-edit-repack patch_mp.ff workflow works.
- The current likely path is carefully verified EBOOT/PRX runtime hooks or another proven runtime script-loading mechanism.

### IW6 split-screen primitives found

A public IW6 Multiplayer/Extinction GSC dump contains mode-level `isSplitScreen()`, per-player `isSplitscreenPlayer()`, and primary-player `isSplitscreenPlayerPrimary()` checks. Extinction also uses `level.players` for player lifecycle and includes a native two-player last-stand/revive flow. This narrows the target from creating an arbitrary second actor to locating the engine-side implementations and local-client setup behind those builtins. The source dump's exact platform/TU is not pinned and it has no Campaign script tree, so these findings are leads, not PS3 Campaign proof. See `docs/iw6-native-splitscreen-reference.md`.

### PS3 executable identity and OPD analysis groundwork

A read-only inspector now accepts a user-supplied decrypted ELF64 big-endian PPU image. It records SHA-256, ELF/load-segment metadata, the entry descriptor, the PS3 PPU OPD table and TOC, and whether the public TU 1.16 function/data candidates map into executable, data or zero-filled BSS segments. It does not decrypt SELF, infer an image base, patch the binary or prove a function is safe. See `docs/ps3-elf-analysis.md`.

A Ghidra script can seed functions from the OPD descriptor table once the same local ELF has been imported as `PowerPC:BE:64:default`. The local-player capture matrix in `docs/local-player-capture-matrix.md` defines what must be compared between an existing offline two-player mode and Campaign.

This tooling is built and unit-tested on synthetic ELF fixtures only. It has not yet been run against the user's exact Ghosts EBOOT because no game executable was available in the development workspace.

### Split-screen identity and clone-path checks

The public TU 1.16 function list includes `PlayerCMD_IsSplitScreenPlayer` and `PlayerCMD_IsSplitScreenPlayerPrimary`, but their PS3 Campaign behavior is unverified. A public PS4 IW6 decompilation shows the first query returning false in that build and the primary query inspecting server-client state, so the names alone are not proof of useful PS3 behavior. The same decompilation shows `PlayerCMD_ClonePlayer` creating a clone/corpse-like entity rather than a second client/player. Do not use it as Player 2. These are research notes, not approved Campaign hooks. See `docs/reverse-engineering-splitscreen.md`.

### Spawn functions

A public TU 1.16 offsets thread lists G_Spawn, G_CallSpawnEntity, PlayerCMD_Spawn and PlayerCMD_ClonePlayer. These addresses are Multiplayer-context leads only. No candidate is approved for Campaign until verified against the exact executable and calling convention.

## Local-player readiness gate added

A pure C gate now defines the evidence required before the runtime adapter can enable the second player: native split-screen mode and player flags, primary/secondary order, controller ports 0/1, distinct local-client/client/entity IDs, Logan/guest-soldier roles, and unique `gclient`, `gentity` and player-state addresses. It fails closed on missing or contradictory observations. The gate is compiled into the PRX and has host tests. It is not called yet because the Campaign frame hook and verified engine observations do not exist. See `docs/local-player-gate.md`.

## Controller input groundwork completed

A host-testable decoder and PS3 `sys_io` adapter have been added for controller ports 0 and 1. The adapter reads `cellPadGetInfo2` and `cellPadGetData`, keeps each port in a separate player snapshot, and maps digital buttons, analog sticks and trigger pressure. It does not initialize or shut down the pad subsystem and does not replace the game's own input path.

Host tests cover button mapping, analog axes, two-port independence, disconnected ports and invalid data lengths. A portable per-frame tracker now also computes pressed/released button edges, normalizes sticks with a deadzone, and preserves the last known state when a port read fails. The PRX exposes these frame values through diagnostic getters. The polling function is still manual, is not called by a Campaign hook, and has not been tested on physical hardware.

## Work sequence

1. **Executable identity:** obtain the exact user-owned TU 1.16 decrypted ELF, record Title ID and SHA-256, run the read-only inspector, establish the OPD range and import the same image into Ghidra.
2. **Baseline local mode:** establish which Ghosts offline mode initializes two local players on the exact PS3 release. Verify the new controller poll on ports 0 and 1, then record controller mapping, player slots, camera/viewports, HUD and restart behavior.
3. **Campaign comparison:** compare one-player Campaign runtime state with the two-player local-mode state. Identify which player/client initialization and split-screen components are missing.
4. **Campaign second slot:** find a safe way to retain or initialize local player slot 2 during Campaign startup.
5. **Player identity:** keep Logan as Player 1 and assign a separate soldier actor to Player 2. Do not substitute a story NPC.
6. **Independent controls:** verify each DualShock controls only its own player, camera, weapon and HUD.
7. **Campaign compatibility:** verify AI targeting, objective triggers, vehicles, cinematics, scripted sequences and checkpoints.
8. **Revive:** connect the existing downed/revive rules to Campaign health, animation and checkpoint failure logic.
9. **Mission coverage:** expand from one mission to all Campaign missions and edge cases.
10. **Online conversion:** only after the local build passes the hardware gate, add remote transport and state synchronization.

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
