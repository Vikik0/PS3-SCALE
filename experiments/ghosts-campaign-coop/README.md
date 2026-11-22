
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
- Ghosts PS3 IW6 FastFiles can be decompressed and inspected; retail files are signed and cannot be rebuilt/re-signed without the publisher's private signing key. The new read-only FastFile inspector and GSC asset exporter are documented in docs/ps3-fastfile-research.md.
- GSC Tool supports IW6 scripts on PS3 for compile/decompile work. This does not itself solve signed FastFile repackaging.
- Public TU 1.16 spawn offsets are multiplayer-context leads only. They are not approved Campaign addresses.

See docs/testing-strategy.md for the researched test pyramid and the rule for when a real PS3 is needed. The new docs/pc-first-test-loop.md explains the Windows host test runner and the optional read-only RPCS3 preflight workflow. See docs/local-coop-first.md for the implementation plan and research constraints. The exact free user-owned input checklist is in docs/user-owned-inputs.md. The first executable profile is now selected as BLUS31270 US disc + TU 1.16 + CEX 4.93 (not hardware-verified); see docs/compatibility-matrix.md. PS3 FastFile/GSC extraction findings and the new read-only tools are documented in docs/ps3-fastfile-research.md. The cross-project IW-engine prior-art audit is in docs/coop-anything-prior-art.md. Newer IW6 script-level split-screen, per-player connect lifecycle, client-ID assignment, HUD scaling and Extinction revive findings are in docs/iw6-native-splitscreen-reference.md. The read-only executable workflow is in docs/ps3-elf-analysis.md, with the comparison checklist in docs/local-player-capture-matrix.md. The structured Clockwork Campaign player-dependency audit (988 functions, 644 direct primary-player references, and per-function hotspots) is in docs/campaign-player-dependency-audit.md; its read-only analyzer and tests are in tools/campaign_player_dependency_audit.py and tests/test_campaign_player_dependency_audit.py. The 121-file TU 1.16 shared/MP script split-screen map is in docs/tu116-splitscreen-script-map.md. The prioritized Clockwork mission player-scoped adaptation matrix is in docs/clockwork-coop-adaptation-plan.md. Phase 7 validation against the user's exact BLUS31270 retail FastFiles and TU 1.16 executables is in docs/phase7-retail-validation.md. Phase 8 adds a manual PS3MAPI Campaign/Multiplayer runtime capture pipeline, a TU 1.16 ELF reconstruction/verifier, a runtime-string scanner, a Campaign/Multiplayer PT_LOAD binary-diff tool, and a separate read-only Pad Probe v4 PRX; see docs/phase8-runtime-capture.md, docs/ps3mapi-campaign-dump-plan.md, and reports/PHASE8_REPORT_AR.md. The 380 extracted ScriptFile candidates from common, Clockwork, and patch FastFiles have also been decompiled and audited for split-screen/co-op handling; findings are in docs/phase8-gsc-splitscreen-audit.md, with a reusable scanner at tools/audit_gsc_splitscreen.py.

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
- Separate read-only Pad Probe v4 PRX: samples cellPadGetInfo2 only, fails closed without an explicit Campaign/Multiplayer mode file, and writes separate logs. It is a controller-port diagnostic, not a local-player spawner.
- PS3MAPI runtime dump/reconstruction tools for the exact TU 1.16 Campaign and Multiplayer SELF PT_LOAD maps, plus runtime string scanning and comparison.
- Reverse-engineering notes rule out treating `PlayerCMD_ClonePlayer` as a full second-player spawn path.
- Read-only PS3 PARAM.SFO inspector: validates PSF metadata and reports Title ID/title/app-version plus SHA-256; it does not infer TU 1.16 from APP_VER.
- Read-only RPCS3 preflight: fingerprints the local PARAM.SFO and EBOOT, checks the expected Title ID, locates the emulator/log, and extracts candidate PPU hashes without modifying or uploading game files.
- Windows host-test runner: compiles and executes the portable C/Python suites and the Ghosts runtime simulator without a PS3.
- Read-only PS3 PPU ELF inspector: validates ELF64 big-endian EM_PPC64, hashes the input, maps reported TU 1.16 function/data candidates including BSS, and extracts the PPU OPD/TOC when present.
- Read-only Ghosts PS3 FastFile inspector: expands IW6 raw-deflate blocks and validated inline zlib assets, with SHA-256 reporting and bounded decompression.
- Ghosts PS3 GSC/CSC asset exporter for already inflated zones, with short/long header recognition, compiled GSC detection, and safe output paths.
- Text-only GSC audit utility that indexes split-screen, player lifecycle, per-client HUD/omnvar, solo/co-op revive branches, engine-player API, checkpoint and mission-flow references.
- Function-level Clockwork Campaign dependency audit: inventories all 11 available mission GSC files, identifies 205 functions containing 644 direct `level.player` references, confirms no mission-local split-screen/`coop` branches in this sample, and ranks the highest-impact player-interaction functions. This is static analysis, not Campaign integration.
- Windows PowerShell batch helper to decompile extracted PS3 GSC binaries with GSC Tool's IW6/PS3 backend.
- Fail-closed local-player readiness gate: requires two distinct native split-screen players, correct primary/secondary identity, controller ports, local-client/client/entity IDs, Logan/guest roles and independent state addresses before a future adapter can enable controls.
- Read-only IW6/PS3 Campaign player registry GSC shim, compiled and round-trip decompiled with GSC Tool 1.6.2.364. It validates an already-existing native split-screen pair; it does not spawn players and has no verified runtime injection path yet.
- Ghidra OPD function-seeding and split-screen builtin cross-reference helpers, plus a local-player state comparison matrix for the next static/runtime analysis stage.

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

CI validates the standalone codecs, controller-input decoder, local-player readiness gate, read-only ELF/OPD, Ghosts FastFile/GSC parser and GSC audit tests, RPCS3 preflight tests, host-only Ghosts runtime contract simulator, and PRX build including `sys_io` imports. A pinned GSC Tool 1.6.2 job also compiles and round-trip decompiles the read-only Campaign player registry shim for IW6/PS3. The host simulator composes the portable modules against a deterministic fake engine boundary and currently runs 10,025 checks plus deliberate test-only fault mutants. The Python suite currently runs 88 tests, including synthetic RPCS3 preflight, Campaign GSC audit and registry-shim contract cases. This is MODELLED contract coverage, not proof that the real game runtime can spawn or render a second Campaign player. The researched test pyramid and real-device gates are in docs/testing-strategy.md; the Phase 1 success gate is defined in docs/local-coop-first.md.

## Important

No retail game files, copyrighted game assets, original EBOOT or installable game PKG are included. The project uses public technical research and will require exact executable identity and runtime validation before enabling any game-memory hooks.
