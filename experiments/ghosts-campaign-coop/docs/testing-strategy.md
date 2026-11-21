# Ghosts co-op test strategy

Purpose: keep rapid iteration on the developer PC and CI. A real PS3 is a hardware-validation target, not the place where every edit is tested.

## What the researched projects actually do

### Co-op Anything

Repository: https://github.com/cranebeaker/coop-anything

Useful patterns:
- `fakeworld`: deterministic in-memory world, virtual clock, seeded RNG, simulated links, state digests, invariants and mutation twins. Its README reports 63 tests / 483 checks / 18 mutation twins in about one second after build.
- `scenarios`: describe a starting state and checks in JSON; validate and dry-run with a fake game before writing a real game adapter.
- `bots`: action adapters and playbooks that check state after each step.
- `bridge`: a game-side main-thread RPC queue with timeouts, state reads, events and screenshots; its own fake game and test server are tested before connecting to the real game.
- `flight`: record events and state digests, compare traces offline, find the first divergence, shrink it and save a regression fixture.
- `mutants`: every important test should be shown to fail when its guarded behavior is deliberately broken.
- `fast-iteration`: keep the game running, use prepared states, event-driven waits and reload only the mod core when the target supports safe hot reload.

### FNV Co-op

Repository: https://github.com/cranebeaker/fnv-coop

This project explicitly separates a headless simulator from two real game processes. The headless `coop-sim` models host/guest logic on a virtual clock and can walk quest rules on the user's game data. `tools/coop-run` starts real viewer processes, runs JSON scenarios, reads logs/state dumps and writes verdicts. Its live control port lets host and guest stay running while scripts send actions and query state. The checklist distinguishes headless-real-logic, modelled stand-ins and real-game evidence; a green headless checklist is not claimed as proof of renderer/engine behavior.

### Modern Warfare 2 Enhanced

Repository: https://github.com/stxlvn/mw2_enhanced

Useful patterns:
- CI runs static patch/script hygiene and builds distributable artifacts without the game.
- `qa_pair.sh` launches two isolated test copies, configures the session and runs named scenarios.
- A watchdog catches startup hangs and stalled logs, requests thread stacks and preserves logs.
- Separate tests check address resolution against known truth values.
- Campaign tours and mission sweeps gather screenshots and logs from selected points.
- The project documents that CI does not prove the patches apply to extracted game scripts or that the mod runs; those require a local game test.

### PS3 tooling

- RPCS3 is an open-source PS3 emulator and debugger with a GDB server. Its official FAQ confirms up to seven simultaneous controller ports, so Player 1 and Player 2 can be driven independently on the PC. A third-party 2026 compatibility record reports BLUS31270 Ghosts reaching L2 BOOTS / exploratory in RPCS3; this is not an official guarantee for our exact TU, settings, or split-screen mode.
- [PS3 C code injection toolkit (ps3-ckit)](https://github.com/tge-was-taken/ps3-ckit) demonstrates a game-specific RPCS3 patch that calls sys_prx_module_load/start and loads a mod.sprx at game boot. Adding a target requires the exact TOC, a safe entrypoint, PRX import addresses and reserved code/data space. Its public targets are Persona 5, Sonic Unleashed and Ratchet & Clank 1 HD; Ghosts is not supported, and the README pins an old RPCS3 build. It is a useful injection design reference, not a drop-in Ghosts loader. Its AGPL-3.0 code should not be copied into this project without an explicit license decision.
- RPCS3 patch files are build-specific: the PPU executable hash and Title ID/version must match. A patch that works for another region or TU is not evidence for BLUS31270 TU 1.16.
- PS3Sharp demonstrates a cross-platform memory-inspection bridge for RPCS3, PS3MAPI (CEX/DEX), TMAPI and CCAPI. Its own README says unit tests are separate from integration tests, which require PS3 or RPCS3.
- webMAN MOD exposes HTTP/PS3MAPI automation: process and memory inspection, module load/unload, game launch, and a virtual-pad command. The documented virtual pad is one emulated pad; it must not be treated as proof that two independent DS3 ports work.

## The proposed Ghosts test pyramid

| Layer | Where | What it proves | Does not prove | Frequency |
|---|---|---|---|---|
| 0. Static and unit | GitHub Actions / host compiler | C/Python logic, parser safety, codecs, input decoding, readiness gate, mission state machine, PRX linking/imports | Ghosts engine accepted the hook | Every commit |
| 1. Fake IW6 runtime contract | Host-only C simulator | Composition of our portable modules with two fake local-player slots; input isolation, distinct camera/HUD identities, AI-target contract, down/revive, checkpoint and transition invariants | Real IW6 memory layout, real spawn, real rendering, actual game scripts | Every commit |
| 2. RPCS3 smoke / integration | Developer PC, user's own game dump | Exact EBOOT/FF behavior under the emulator, debugger observations, crash/boot checks, possible rapid PRX/hook iteration if injection is proven | Physical PS3 timing/CFW behavior or guaranteed controller parity | At each engine-integration milestone; later per build if hot reload is proven |
| 3. Physical PS3 validation | Actual CEX 4.93 / Ghosts TU 1.16 target | Real controller ports, actual split-screen initialization, Campaign camera/HUD, gameplay, thermals and long-run stability | Nothing else can substitute for this final hardware gate | Only at milestone/release gates, not every code edit |

## Why the PS3 should not be used for every edit

Most of the current project is already portable C/Python. The input decoder/tracker, local-player gate, mission rules, packet codecs, GSC/ELF/FastFile parsers and protocol contracts can be compiled and tested on Windows/Linux CI. The PRX bootstrap can also be cross-compiled in GitHub Actions. Keep the actual PS3 engine adapter as a very thin layer around those tested components.

New `tests/ghosts_runtime_sim_test.c` composes the existing portable input decoder/tracker, local-player readiness gate and mission rules with a deterministic fake engine boundary. It checks controller-to-player isolation, distinct camera/HUD contexts, AI selection of either player, down/revive, checkpoint restoration and mission-transition identity. It also includes deliberate test-only mutants (cross-routed input, shared camera/HUD, P1-only AI, dropped P2 at checkpoint, duplicate P2 at transition, aliased player state) to prove the oracles catch those classes of bug.

**Fidelity warning:** the fake runtime is a contract simulator. It tests our portable modules and the invariants expected from a future Ghosts adapter. It does not emulate IW6, create a real game player, or validate any game address. Its results must be labelled `MODELLED`, not `REAL GAME`.

## RPCS3 lane: decision and limits

Use RPCS3 as the first real-game test environment if the user's own BLUS31270/TU 1.16 dump boots. Configure two independent controller ports and first verify the existing offline split-screen baseline. Then prove one game-specific PRX load route. The leading candidate is an original, Ghosts-specific RPCS3 patch that invokes sys_prx_module_load/start at boot, following the architectural pattern documented by ps3-ckit; we must derive all addresses and import stubs from the user's exact EBOOT, not copy Persona 5/Sonic/Ratchet values. Keep one persistent game installation in RPCS3; do not reinstall or re-extract the ISO for every change. Update only the development PRX/patch artifact and restart the emulated game. Safe in-process module unload/reload is a later optimization, not assumed.

Before calling RPCS3 a reliable lane, verify in order:
1. The exact user-owned Title ID and TU boot to Campaign.
2. The same build boots an existing two-player local mode with two distinct emulated controller ports.
3. The development PRX can be loaded through a verified Ghosts-specific patch/loader. A full game restart is acceptable at first; safe module unload/reload is optional and must be proven.
4. The emulator's memory/debugger observations match the expected addresses and per-player identities.
5. Repeat a short mission, checkpoint and restart scenario; compare logs and screenshots.

RPCS3 is a high-value development proxy and may let us test two local players on the PC, but it is not the final authority. Do not reuse its memory addresses as PS3 physical addresses without validation; do not infer CEX behavior, RSX timing or physical controller behavior from emulator success.

## Optional remote PS3 automation

If webMAN MOD / PS3MAPI is already enabled on the target CEX console, a PC runner can potentially transfer a test PRX, load/unload the module, read process/memory state, launch the mounted game and collect logs without manually navigating XMB. The webMAN virtual pad can automate simple menu/button actions. Keep this optional until a read-only preflight and a harmless module load/unload test are proven. Never automate writes to LV1/LV2 or arbitrary game memory during this setup.

Important limitation: webMAN's documented virtual pad is one virtual controller, not two separately addressable DualShock 3 ports. A real two-controller Campaign test still needs two actual controllers or a separately validated multi-pad input solution.

## Practical iteration rule

- Edit portable code -> host tests + CI only.
- Edit GSC/ELF/FastFile parsers -> synthetic fixtures + real user-owned file regression fixtures (hashes/metadata only; do not commit game assets).
- Edit engine adapter / hook / addresses -> RPCS3 integration when available.
- Change renderer, local-player init, controller routing, checkpoint or mission transition -> one milestone test on the physical PS3.
- Release candidate -> full physical two-controller smoke test and the 20-minute stability gate.

Do not package an installable PKG for every change. Keep CI artifacts as PRX/ELF diagnostics; create an installable package only when a milestone is ready for a real-device gate.