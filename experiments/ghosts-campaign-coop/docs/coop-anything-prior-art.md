# Co-op Anything prior-art audit

Checked: 2026-11-21  
Purpose: identify reusable research and engineering methods for the PS3 Call of Duty: Ghosts local Campaign co-op experiment.

## Repository identified

The repository the owner meant is almost certainly [cranebeaker/coop-anything](https://github.com/cranebeaker/coop-anything).

It is a shared co-op engineering playbook, agent skills, an MCP server, a directory of existing co-op projects, and game-agnostic testing tools. It is **not** a ready-made Ghosts mod or a binary patch that can simply be installed.

No Call of Duty: Ghosts-specific project or write-up was found in its current game directory during this review. It does contain useful prior art for other Call of Duty games, especially Modern Warfare 2 (2009) and Modern Warfare 3 (2011).

## Most relevant prior art: Modern Warfare 2 Enhanced

Sources:
- [Co-op Anything: Modern Warfare 2 write-up](https://github.com/cranebeaker/coop-anything/blob/main/games/modern-warfare-2/README.md)
- [MW2 Enhanced source](https://github.com/stxlvn/mw2_enhanced)
- [MW2 Enhanced native split-screen study](https://github.com/stxlvn/mw2_enhanced/blob/main/docs/native-splitscreen-360.md)
- [MW2 Enhanced split-screen plan](https://github.com/stxlvn/mw2_enhanced/blob/main/docs/native-splitscreen-plan.md)
- [MW2 Enhanced client send research](https://github.com/stxlvn/mw2_enhanced/blob/main/docs/native-splitscreen-research/coop-client-send.md)
- [MW2 Enhanced host frame research](https://github.com/stxlvn/mw2_enhanced/blob/main/docs/native-splitscreen-research/coop-host-frame.md)
- [MW2 Enhanced slot-1 snapshot research](https://github.com/stxlvn/mw2_enhanced/blob/main/docs/native-splitscreen-research/coop-snapshot-slot1.md)

The Co-op Anything write-up reports that MW2's single-player executable already contains a two-player Spec Ops co-op/lockstep path. The MW2 Enhanced project reuses that path, loads Campaign maps in the co-op session, and modifies GSC mission logic for two players. Its Campaign work includes checkpoints, down/revive, mission transitions, vehicles, cutscenes and mission-specific set pieces.

This is valuable because Ghosts is also an Infinity Ward Call of Duty title. It gives us a concrete example of how an IW-family game can separate:
- existing engine-level co-op/local-player machinery;
- Campaign script assumptions that still expect one player;
- per-mission fixes for vehicles, scripted scenes, objectives and checkpoints.

It does **not** prove that Ghosts has the same co-op entry points, script behavior, player structures or network protocol. MW2 is IW4; Ghosts is IW6, and our target is the PS3 PPU executable. MW2 addresses, structure offsets, GSC patches and PC hooks must not be copied into Ghosts.

## Most relevant split-screen finding

The MW2 Enhanced research compares the PC game with the Xbox 360 version. Its 360 reference shows a genuine per-local-player architecture:

- each local player has its own cgame/client state;
- each player is drawn in a separate view/viewport during the same frame;
- the renderer selects per-player view state;
- entity identity, field of view, sound listener and effects have per-player aspects.

The PC MW2 build had some of that machinery reduced to a single local client, so MW2 Enhanced had to reconstruct and test a second client context. Its long research log documents bugs involving renderer frame data, viewport clears, snapshots, player entity slots, animation stepping, effects and HUD state.

### Consequence for Ghosts

Our first choice should remain to reuse the **actual PS3 Ghosts local-player path** that is already used by a supported local mode, if the target build exposes one. We should not recreate the MW2 PC split-screen architecture unless Ghosts' own PS3 executable proves that its native path is missing or unusable.

The next reverse-engineering comparison should capture a working Ghosts PS3 local two-player session and a Campaign session, then identify the differences in:
1. local client/player count and slot initialization;
2. controller-to-local-player assignment;
3. player entity and player-state ownership;
4. camera, viewport and per-player HUD setup;
5. snapshot/user-command flow and local input routing;
6. mode teardown, respawn, checkpoint and restart behavior.

The current PRX `cellPadGetInfo2` / `cellPadGetData` reader remains an unhooked diagnostic adapter only. If the game already routes each controller into its own local-client user-command stream, Campaign integration should use that native route rather than introducing a competing input poll.

## Co-op Anything methods worth adopting

The repository's skills and playbooks provide a useful workflow:

- **Triage and identity:** record the exact title update, executable hash, platform and region before applying any findings.
- **Reverse engineering:** prefer existing source/decompilation, label observations separately from inference, and keep private dumps out of public commits.
- **Build from zero:** prove a second player/client exists before implementing story sync.
- **QA:** test the exact mode/player count first, then automate movement, damage, down/revive, checkpoint and restart scenarios.
- **Sync tools:** `fakeworld` and `fuzz` can help test portable mission/state logic; `flight`, `ownership`, `bots`, `scenarios` and `animcheck` become useful as soon as a real game-state bridge or multi-client sync path exists.

The generic sync tools are not PS3-specific. Any adopted tool needs a small adapter and a build/test check in this repository. No Co-op Anything tool has been installed or imported into the project in this audit.

## Important platform caveat

Co-op Anything's generic console playbook mostly recommends a decomp/recompiled PC port or emulator-based co-op for console-only titles. That is not our agreed Phase 1 target. We are intentionally targeting two players on the **actual PS3**, offline, using Ghosts' own local-player facilities if available. We use the repository for prior art, reverse-engineering discipline and QA methods, not as a reason to change the platform target.

## Current decision

Use Co-op Anything as a research and QA reference. Do not fork or copy MW2 code into this project, do not reuse its addresses, and do not claim Ghosts Campaign co-op works based on MW2 results.

The next implementation gate is still: prove and document the native second local-player/client path in the exact Ghosts PS3 build before adding a Campaign Player 2 spawn or camera hook.
