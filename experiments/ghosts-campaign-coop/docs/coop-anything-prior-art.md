# Co-op Anything prior-art audit

Checked: 2026-11-21
Purpose: identify reusable research and engineering methods for the PS3 Call of Duty: Ghosts local Campaign co-op experiment.

## Repository identified

The repository the owner meant is almost certainly [cranebeaker/coop-anything](https://github.com/cranebeaker/coop-anything).

It is a shared co-op engineering playbook, agent skills, an MCP server, a directory of existing co-op projects, and game-agnostic testing tools. It is **not** a ready-made Ghosts mod or a binary patch that can simply be installed.

No Call of Duty: Ghosts-specific project, issue, or write-up was found in its current game directory during this review. It does contain useful prior art for other Call of Duty games, especially Modern Warfare 2 (2009) and Modern Warfare 3 (2011).

## Most relevant prior art: Modern Warfare 2 Enhanced

Sources:
- [Co-op Anything: Modern Warfare 2 write-up](https://github.com/cranebeaker/coop-anything/blob/main/games/modern-warfare-2/README.md)
- [MW2 Enhanced source](https://github.com/stxlvn/mw2_enhanced)
- [MW2 Enhanced native split-screen study](https://github.com/stxlvn/mw2_enhanced/blob/main/docs/native-splitscreen-360.md)
- [MW2 Enhanced split-screen plan](https://github.com/stxlvn/mw2_enhanced/blob/main/docs/native-splitscreen-plan.md)
- [MW2 Enhanced client send research](https://github.com/stxlvn/mw2_enhanced/blob/main/docs/native-splitscreen-research/coop-client-send.md)
- [MW2 Enhanced host frame research](https://github.com/stxlvn/mw2_enhanced/blob/main/docs/native-splitscreen-research/coop-host-frame.md)
- [MW2 Enhanced slot-1 snapshot research](https://github.com/stxlvn/mw2_enhanced/blob/main/docs/native-splitscreen-research/coop-snapshot-slot1.md)
- [Current MW2 Enhanced README and release notes](https://github.com/stxlvn/mw2_enhanced)

The Co-op Anything write-up reports that MW2's single-player executable already contains a two-player Spec Ops co-op/lockstep path. The MW2 Enhanced project reuses that path, loads Campaign maps in the co-op session, and modifies GSC mission logic for two players. Its Campaign work includes checkpoints, down/revive, mission transitions, vehicles, cutscenes and mission-specific set pieces.

The source audit adds an important distinction: MW2 Enhanced's network Campaign co-op and its local split-screen are related but separate paths. The former reuses the game's Spec Ops lockstep netcode. The latter is a native local mode in one PC process: it establishes a second local client context, routes a separate input device, steps the second client's game logic, and draws a second view. The implementation patches a local-player count and keeps per-player client/cgame state separate. This is a detailed engineering precedent, not a Ghosts patch.

This is valuable because Ghosts is also an Infinity Ward Call of Duty title. It gives us a concrete example of how an IW-family game can separate:
- existing engine-level co-op/local-player machinery;
- Campaign script assumptions that still expect one player;
- per-mission fixes for vehicles, scripted scenes, objectives and checkpoints.

It does **not** prove that Ghosts has the same co-op entry points, script behavior, player structures or network protocol. MW2 is IW4; Ghosts is IW6, and our target is the PS3 PPU executable. MW2 addresses, structure offsets, GSC patches and PC hooks must not be copied into Ghosts.

## Ghosts: evidence for a native two-player baseline

The strongest route is to inspect Ghosts' own existing local-player path rather than inventing a second actor.

- PS3 player reports describe Multiplayer, Squads and Extinction as playable offline with one or two local players; another PS3 answer explicitly reports two-player split-screen. These are community reports, not executable-level proof: [GameFAQs PS3 offline multiplayer answer](https://gamefaqs.gamespot.com/ps3/703505-call-of-duty-ghosts/answers/361457-can-the-multiplayer-played-in-offline), [GameFAQs PS3 offline features answer](https://gamefaqs.gamespot.com/ps3/703505-call-of-duty-ghosts/answers/415040-what-i-can-do-without-internet-connection).
- PS3 TU patch notes include a fix for a split-screen UI issue, confirming that the PS3 build has native split-screen UI paths: [PlayStationTrophies, PS3 update notes](https://www.playstationtrophies.org/news/news-11737-call-of-duty-ghosts-updated-on-ps3-ps4.html).
- Activision describes Extinction as a separate four-player co-op mode, not as the Campaign: [official Ghosts page](https://www.callofduty.com/ghosts).
- The campaign is documented as single-player; the existence of local co-op in other modes does not imply Campaign can load a second player: [Ghosts campaign FAQ](https://codghost.net/faq/).

These reports make Extinction or a local Squads/Multiplayer mode the best runtime baseline to capture on the exact PS3 TU 1.16 build. We must first verify which of those modes actually creates two local clients on the selected title ID. Do not assume the GameFAQs report applies to every region, update, or console configuration.

## Most relevant split-screen finding

The MW2 Enhanced research compares the PC game with the Xbox 360 version. Its 360 reference shows a genuine per-local-player architecture:

- each local player has its own cgame/client state;
- each player is drawn in a separate view/viewport during the same frame;
- the renderer selects per-player view state;
- entity identity, field of view, sound listener and effects have per-player aspects.

The PC MW2 build had some of that machinery reduced to a single local client, so MW2 Enhanced had to reconstruct and test a second client context. Its research log documents bugs involving renderer frame data, viewport clears, snapshots, player entity slots, animation stepping, effects and HUD state.

### Consequence for Ghosts

Our first choice should remain to reuse the **actual PS3 Ghosts local-player path** that is already used by a supported local mode, if the target build exposes one. We should not recreate the MW2 PC split-screen architecture unless Ghosts' own PS3 executable proves that its native path is missing or unusable.

The next reverse-engineering comparison should capture a working Ghosts PS3 local two-player session and a Campaign session, then identify the differences in:
1. local client/player count and slot initialization;
2. controller-to-local-player assignment;
3. player entity and player-state ownership;
4. camera, viewport and per-player HUD setup;
5. snapshot/user-command flow and local input routing;
6. mode teardown, respawn, checkpoint and restart behavior.

The current PRX cellPadGetInfo2 / cellPadGetData reader remains an unhooked diagnostic adapter only. If the game already routes each controller into its own local-client user-command stream, Campaign integration should use that native route rather than introducing a competing input poll.

## Co-op Anything console playbook: what applies and what does not

The hub's console guidance offers four routes: use a decomp/recompiled PC port if one exists; use emulator input-sync when the game already has local multiplayer; use memory-sync between emulator instances when it does not; or use remote play. Its PS3 row names RPCS3/RPCN for existing online-capable game modes, says no PINE support was found, and labels external PS3 emulator memory access as unverified. Its decomp/recomp guide rates PS3 decomp/recomp as very high difficulty and lists no finished public PC port.

That guidance is useful for comparing options, but it does not meet the agreed Phase 1 requirement: **two controllers on one physical PS3, offline, without PSN or a PC emulator**. RPCN restores existing online modes; it does not create a second Campaign player. Emulator memory-sync would change the platform and runtime. We therefore keep both as out-of-scope fallbacks, not the implementation plan.

## Co-op Anything methods worth adopting

The repository's skills and playbooks provide a useful workflow:

- **Triage and identity:** record the exact title update, executable hash, platform and region before applying any findings.
- **Reverse engineering:** prefer existing source/decompilation, label observations separately from inference, and keep private dumps out of public commits.
- **Build from zero:** prove a second player/client exists before implementing story sync.
- **QA:** test the exact mode/player count first, then automate movement, damage, down/revive, checkpoint and restart scenarios.
- **Sync tools:** fakeworld and fuzz can help test portable mission/state logic; flight, ownership, bots, scenarios and animcheck become useful as soon as a real game-state bridge or multi-client sync path exists.
- **Network chaos:** chaos is useful only if/when we later add online Phase 2. It is not needed for Phase 1 local play.

The generic sync tools are not PS3-specific. Any adopted tool needs a small adapter and a build/test check in this repository. No Co-op Anything tool has been installed or imported into the project in this audit.

## Intake tooling added

A free, read-only tools/ps3_sfo_inspect.py parser now reads the user's local PARAM.SFO, reports TITLE_ID, TITLE, APP_VER, VERSION, file size and SHA-256, and identifies whether the metadata matches the selected BLUS31270 profile. It deliberately does not claim that APP_VER proves TU 1.16; the installed update and executable must be checked separately. Synthetic parser tests cover valid metadata, malformed offsets, invalid keys and wrong data types.

## Important platform caveat

Co-op Anything's generic console playbook mostly recommends a decomp/recompiled PC port or emulator-based co-op for console-only titles. That is not our agreed Phase 1 target. We are intentionally targeting two players on the **actual PS3**, offline, using Ghosts' own local-player facilities if available. We use the repository for prior art, reverse-engineering discipline and QA methods, not as a reason to change the platform target.

## Current decision

Use Co-op Anything as a research and QA reference. Do not fork or copy MW2 code into this project, do not reuse its addresses, and do not claim Ghosts Campaign co-op works based on MW2 results.

The next implementation gate is still: prove and document the native second local-player/client path in the exact Ghosts PS3 build before adding a Campaign Player 2 spawn or camera hook.