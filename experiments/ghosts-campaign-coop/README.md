
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

See docs/testing-strategy.md for the researched test pyramid and the rule for when a real PS3 is needed. The new docs/pc-first-test-loop.md explains the Windows host test runner and the optional read-only RPCS3 preflight workflow. See docs/local-coop-first.md for the implementation plan and research constraints. The exact free user-owned input checklist is in docs/user-owned-inputs.md. The first executable profile is now selected as BLUS31270 US disc + TU 1.16 + CEX 4.93 (not hardware-verified); see docs/compatibility-matrix.md. PS3 FastFile/GSC extraction findings and the new read-only tools are documented in docs/ps3-fastfile-research.md. The cross-project IW-engine prior-art audit is in docs/coop-anything-prior-art.md. Newer IW6 script-level split-screen, per-player connect lifecycle, client-ID assignment, HUD scaling and Extinction revive findings are in docs/iw6-native-splitscreen-reference.md. The read-only executable workflow is in docs/ps3-elf-analysis.md, with the comparison checklist in docs/local-player-capture-matrix.md. The structured Clockwork Campaign player-dependency audit (988 functions, 644 direct primary-player references, and per-function hotspots) is in docs/campaign-player-dependency-audit.md; its read-only analyzer and tests are in tools/campaign_player_dependency_audit.py and tests/test_campaign_player_dependency_audit.py. The 121-file TU 1.16 shared/MP script split-screen map is in docs/tu116-splitscreen-script-map.md. The prioritized Clockwork mission player-scoped adaptation matrix is in docs/clockwork-coop-adaptation-plan.md. Phase 7 validation against the user's exact BLUS31270 retail FastFiles and TU 1.16 executables is in docs/phase7-retail-validation.md. Phase 8 adds a manual PS3MAPI Campaign/Multiplayer runtime capture pipeline, a TU 1.16 ELF reconstruction/verifier, a runtime-string scanner, a Campaign/Multiplayer PT_LOAD binary-diff tool, and a separate read-only Pad Probe v4 PRX; see docs/phase8-runtime-capture.md, docs/ps3mapi-campaign-dump-plan.md, and reports/PHASE8_REPORT_AR.md. Phase 9 documents the newly verified Campaign/shared GSC second-player entity path and the Multiplayer callback/client-ID lifecycle; see docs/phase9-gsc-player-lifecycle.md, reports/PHASE9_REPORT_AR.md, and reports/PHASE9_GSC_EVIDENCE.json. The 380 extracted ScriptFile candidates from common, Clockwork, and patch FastFiles have also been decompiled and audited for split-screen/co-op handling; findings are in docs/phase8-gsc-splitscreen-audit.md, with a reusable scanner at tools/audit_gsc_splitscreen.py. Phase 10 adds one local command to validate both TU 1.16 runtime captures, rebuild analysis ELFs, compare strings and PT_LOAD blocks, and optionally assess paired Pad Probe logs; see docs/phase10-capture-intake.md and reports/PHASE10_REPORT_AR.md. Phase 11 adds a Campaign local-session bridge that reuses native Multiplayer split-screen identities, binds Campaign's primary/secondary player aliases, and exposes a Clockwork Player 2 starting-loadout adapter; see docs/phase11-native-multiplayer-reuse.md and reports/PHASE11_REPORT_AR.md.

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
- Separate read-only Pad Probe v4 PRX: samples cellPadGetInfo2 only, fails closed without an explicit Campaign/Multiplayer mode file, and writes separate logs. The comparator now requires the v4 read-only header, all six periodic checkpoints, the completion marker, unique in-range sample indices, and verified operator-selected mode provenance before a capture pair can pass. It is a controller-port diagnostic, not a local-player spawner.
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
- Fail-closed local-player readiness gate: requires two native split-screen identities, correct primary/secondary roles, controller ports, distinct local-client/client/entity IDs, Logan/guest roles, fresh input frames from the same valid engine frame, independent command buffers, separate player-state/view/HUD contexts, and canonical boolean evidence before a future adapter can enable controls.
- Read-only IW6/PS3 Campaign player registry GSC shim, compiled and round-trip decompiled with GSC Tool 1.6.2.364. It validates an already-existing native split-screen pair; it does not spawn players and has no verified runtime injection path yet.
- Campaign local-session GSC bridge that reuses native Multiplayer split-screen identities, retries bounded startup binding, revalidates both live native players against the current entity array before reuse or Campaign actions, binds level.player to the native primary and level.playengineeffects to the native secondary, and provides a Clockwork Player 2 starting-loadout helper. It is compile-tested source only; native Campaign client creation and runtime script loading are still unverified.
- Portable Campaign interaction contract for Clockwork's two sequential drill stages: one global owner at a time, Stage 2 requires Stage 1 completion, and either local player may own either stage. Death/disconnect/cancel releases the active stage without committing completion. Host tests cover stage order and ownership.
- Read-only Clockwork drill source audit: verifies direct trigger-activator capture, native secondary identity, explicit player parameters, per-player ammo/aim state, player-pair coordination, death retry cleanup, original ammo-counter dvar restoration, and the pinned six-function source baseline. See docs/phase13-clockwork-drill-source-audit.md and tools/clockwork_drill_adapter_audit.py.
- Local Clockwork Stage 2 patch record: direct trigger-touch capture, death retry, removal of the blocking flag wait, and original ammo-counter dvar restoration. The local GSC source compiled and round-tripped to byte-identical GSCBIN (22,902 bytes); game-derived source and bytecode are not committed here. See docs/phase14-clockwork-trigger-recovery.md.
- Local Clockwork Stage 3 patch record: `handle_drill_spot` now returns the actual player who completed each stage, and the outer `back1`/`back2` animations plus drill plant/pullout sounds follow that owner. The local GSC source compiles and round-trips to byte-identical GSCBIN. The drill PIP still uses the stock shared `level.pintoscreenedge` path and needs separate HUD/runtime validation. See docs/phase16-clockwork-owner-handoff.md.
- Latest CI validation: run `45070589371` passed all five jobs; 197 Python tests passed, including sequential interaction ownership and owner-specific drill animation/audio checks. This does not replace a PS3/RPCS3 runtime test.
- Local Clockwork Stage 4 PIP patch: `pip_enable` now accepts the active drill player, creates/reuses that player's `newpip` object, stops stale PIP threads on reuse/rebind, and the drill timer receives `drill_player`. Both local GSC scripts compile and round-trip byte-identically. Actual per-viewport HUD rendering still needs PS3/RPCS3 validation. See docs/phase17-clockwork-pip-owner.md and the read-only audit in tools/clockwork_pip_owner_audit.py.
- Latest CI run `45071437477`: all five jobs passed; 202 Python tests passed, including the PIP-owner audit and its negative tests. No PS3/RPCS3 runtime test is claimed.
- Local Clockwork Stage 5 patch: vault blast damage, shellshock, prone stance, push vector and artillery rumble are now routed to both valid local players. The interior GSCBIN compiles and round-trips byte-identically. A new source audit blocks primary-only blast handling. See docs/phase18-clockwork-vault-blast.md.
- Local Clockwork Stage 6 patch: secondary-player interior loadout/NVG, vault hints, weapon-switch waiting, grenade save/restore bound to the same entity, CQB sprint/jump and dog-attack notification are now covered. The separate interior NVG lever and AI target selection remain open. See docs/phase19-clockwork-interior-player-sync.md.
- Local Clockwork Stage 7 / Phase 20 patch: the no-argument NVG lever entry point now routes a player-owned flashlight FX to Logan and the validated native secondary player, watches for late secondary binding, tracks NVG state per player and cleans up on lights-on/death. The pinned source patcher and 20-check audit are in tools/clockwork_nvg_player_patch.py and tools/clockwork_nvg_player_audit.py; 18 new host tests pass. The synthetic helper fixture is added to the pinned GSC Tool CI compile/decompile round-trip. Local retail-source GSCBIN round-trip is byte-identical; live PS3/RPCS3 viewport and Player 2 validation remain open. See docs/phase20-clockwork-nvg-player-sync.md and reports/PHASE20_REPORT_AR.md.
- Local Clockwork Stage 8 / Phase 21 AI targeting pass: audited the single direct enemy-to-Logan assignment in `discovery_guys_wakeup`, distinguished it from shared ally-to-enemy `attack_targets` and primary-only dialogue/audio helpers, and added a function-body-pinned patcher that selects the nearest valid local player once per awakened AI. The secondary alias is now accepted only when `issplitscreen()` is true and the entity passes `issplitscreenplayer()` but not `issplitscreenplayerprimary()`. The historical merged Stage 5/6 source was not retained, so Phase 23 reconstructs the documented Phase 18/19 behaviors on the pinned Stage 2 source before applying Stage 8. Stage 5/6 reconstruction audit passes 13/13, its host tests pass 7/7, and Stage 8 host tests pass 11/11. Stage 5, Stage 6 and combined Stage 8 GSC Tool compile/decompile/recompile round-trips are byte-identical. The reconstruction is behavior-based, not byte-identical to the old Phase 18/19 binaries; live runtime aggro remains untested. See docs/phase21-clockwork-ai-targeting.md, docs/phase23-clockwork-assets-stage56-reconstruction.md, reports/PHASE21_REPORT_AR.md and reports/PHASE23_REPORT_AR.md.
- Phase 22 native-session lifecycle monitor: the Campaign local-session bridge now starts one guarded watcher after a successful pair bind/reuse, revalidates the exact primary/secondary native entities and Campaign aliases every 250 ms, tolerates seven transient failures, then invalidates stale aliases and releases active drill ownership after the eighth failure (~2 s). Five new host source-contract tests cover the monitor. It does not spawn or rebind Player 2; after status 8, the bridge `main()` must be invoked again. See docs/phase22-native-session-monitor.md and reports/PHASE22_REPORT_AR.md.
- Phase 23 uploaded Clockwork asset intake: verified the user's `clockwork(1).ff` against the previously audited FastFile, decompiled `maps/clockwork_interior` byte-identically, classified the transition/localization FFs and five Bink clips, and confirmed that the MapEnts entity string is still absent. Reconstructed Stage 5/6 from their Phase 18/19 specifications on top of the pinned Stage 2 source, then applied Stage 8; the reconstructed builds compile and round-trip byte-identically. This is a behavior-based reconstruction, not recovery of the historical Stage 5/6 binaries. See docs/phase23-clockwork-assets-stage56-reconstruction.md, reports/PHASE23_REPORT_AR.md and reports/phase23-uploaded-asset-inventory.json.
- Phase 24 Clockwork cinematic ownership guard: the drill source audit now requires exactly one source call site for the mission-level `clockwork_vault_scan` and `clockwork_vault_drill` Bink events, and blocks missing or duplicated calls. Tests verify that Stage 5/6 reconstruction preserves exactly one call site per event. The synthetic Stage 2 patcher-input fixture and synthetic Stage 5/6 GSC Tool fixture are separate from each other and from retail source. See docs/phase24-clockwork-cinematic-ownership.md and reports/PHASE24_REPORT_AR.md.
- Phase 25 Pad Probe capture integrity gate: the comparator rejects truncated v4 logs, missing five-sample-interval checkpoints, duplicate/out-of-range indices, missing completion markers, and unverified operator-selected mode metadata. The TU 1.16 capture analyzer now requires both logs to pass this gate before marking the capture pair ready for engine review. See docs/phase25-pad-capture-integrity.md and reports/PHASE25_REPORT_AR.md.
- Phase 26 PPU call-target inventory: Ghidra now exports resolved call-target symbols alongside addresses, and the Campaign/Multiplayer inventory comparer ranks Multiplayer-only leads using both function names and called-symbol names while retaining compatibility with legacy CSV files. This is static-analysis triage only; no hook is approved. See docs/phase26-ppu-call-target-inventory.md and reports/PHASE26_REPORT_AR.md.
- Phase 27 PPU call-graph comparison: Ghidra now exports resolved internal call-target function entries. The comparer builds Campaign/Multiplayer call graphs, reports callee-fingerprint changes for matching functions, and ranks opaque Multiplayer-only functions using incoming callers as well as outgoing symbol evidence. Reports include resolved callee names/entries, caller context, and shared-vs-Multiplayer-only target fingerprints; duplicate function entries are rejected and legacy CSV remains supported. This is static-analysis triage only. See docs/phase27-ppu-call-graph-comparison.md and reports/PHASE27_REPORT_AR.md.
- Phase 28 Ghidra headless runner: one host-side command imports the exact Campaign and Multiplayer analysis ELFs into separate temporary Ghidra projects, applies the PowerPC:BE:64:default processor, optionally seeds OPD functions from explicitly supplied inspector values, exports and validates both inventories, and writes a SHA-256 manifest. It stages all outputs and refuses partial promotion or accidental overwrite. CI uses a mocked Ghidra process; actual Ghidra execution still requires the operator's local installation and verified TU 1.16 ELFs. See docs/phase28-ghidra-headless-runner.md and reports/PHASE28_REPORT_AR.md.
- Phase 29 PPU inventory manifest verification: the comparer accepts the Phase 28 manifest and verifies the declared BLUS31270/TU 1.16/PowerPC profile, CSV filenames, CSV SHA-256 values, function counts and source ELF hash fields before producing a comparison. Legacy comparison remains possible but is explicitly marked as lacking provenance validation. See docs/phase29-ppu-inventory-manifest-verification.md and reports/PHASE29_REPORT_AR.md.
- Phase 33 player-state ownership audit: classifies player method calls, player fields, player-array references, secondary-client references, level-scope fields, flags and dvars across 17 high-priority functions in Black Ice, Carrier and Cornered. It explicitly separates level.playerpiggyback from level.playengineeffects and warns against cloning mission-level state per player. See docs/phase33-player-state-ownership.md and reports/PHASE33_REPORT_AR.md.
- Phase 32 GSC call-site and callback reachability audit: traces direct calls, thread calls, namespaced calls and function-pointer registrations across the 249 Phase 31 mission scripts. It distinguishes direct reachability from callback-inclusive paths, records ambiguous symbol resolution instead of guessing, and confirms which player-array-aware paths are actually referenced inside the extracted scripts. The report does not claim runtime execution or a native Campaign second client. See docs/phase32-gsc-callgraph-audit.md and reports/PHASE32_REPORT_AR.md.
- Phase 30 full USRDIR inventory: added an offline recursive scanner for every .ff, .d3dbsp, .ents, .zone and extracted GSC candidate under PS3_GAME/USRDIR. It records SHA-256 and FastFile parse status, inspects ScriptFile/RawFile/MapEnts pool records, inventories script names, finds map-path references and scans standalone map/entity files. Unsupported containers are reported rather than silently skipped. This phase adds the scanner and synthetic tests; it does not claim the entire local USRDIR was scanned by CI. See docs/phase30-usrdir-fastfile-inventory.md and reports/PHASE30_REPORT_AR.md.
- Phase 31 three-mission audit: extracted 249 compiled GSC candidates from the supplied Black Ice, Carrier and Cornered FastFiles; all 249 decompiled and recompiled with GSC Tool 1.6.2.364. Static audit covers 55 map scripts, 2,255 direct level.player references and existing player-array paths in Carrier/Cornered; two Carrier helper call paths remain unconfirmed by static cross-file search. FastFile trailing regions and the separate .d3dbsp map data remain unresolved; no playable Campaign co-op claim is made. See docs/phase31-three-mission-audit.md and reports/PHASE31_REPORT_AR.md.
- PS3 MapEnts inspector: read-only scanner for the external IW6 PS3 map `.d3dbsp` entity string. It identifies classnames and targetnames (including the two Clockwork drill triggers) without modifying the map file. See docs/phase15-clockwork-mapents.md and tools/ps3_mapents_inspect.py.
- Phase 14 applies a player-routed adaptation to the user's actual `maps__clockwork_interior.gsc` source and successfully compiles/decompiles it with GSC Tool 1.6.2.364. The local output hashes and changes are recorded in docs/phase14-clockwork-drill-player-routing.md. The modified GSCBIN is not yet inserted into `clockwork.ff`, and no PS3/RPCS3 runtime test or installable PKG exists.
- Phase 15 adds a read-only PRX GSCBIN loader: it reads `/dev_hdd0/tmp/ghosts/maps__clockwork_interior.gscbin`, validates the GSC wrapper lengths, and exposes status/length/address diagnostics. It does not hook or write game memory. `tools/ps3mapi/scan_clockwork_script_asset.py` locates the original ScriptFile in a raw zone or memory dump and builds a replacement asset blob for size analysis. The replacement is 1,658 bytes larger than the original, so a verified engine allocation/registry path is required. See docs/phase15-runtime-script-override-preparation.md.
- Ghidra/Python PPU function-inventory workflow for comparing Campaign and Multiplayer builds and ranking native local-client/player function leads without guessing offsets. The Ghidra exporter is a manual read-only analysis script; the Python comparator is host-tested.
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

CI validates the standalone codecs, controller-input decoder, local-player readiness gate, read-only ELF/OPD, Ghosts FastFile/GSC parser and GSC audit tests, RPCS3 preflight tests, host-only Ghosts runtime contract simulator, and PRX build including `sys_io` imports. A pinned GSC Tool 1.6.2 job compiles and round-trip decompiles both the read-only Campaign player registry shim and the native Multiplayer-to-Campaign local-session bridge for IW6/PS3. The Phase 12 bridge GSCBIN is 2,372 bytes (SHA-256 545c064f21aad28c5d3d636adaf4d8b1009da852a446a394267e54570c1b44b2); its verified source SHA-256 is ae46676fb265aa334759e7700a7f2a9b7c305e2c6868393afddbf94421b4c58f. The latest verified bridge artifact is produced by the pinned GSC Tool 1.6.2 CI job. See docs/phase12-clockwork-drill-adapter.md and reports/PHASE12_REPORT_AR.md for the current drill-ownership adapter. The host simulator composes the portable modules against a deterministic fake engine boundary and currently runs 10,025 checks plus deliberate test-only fault mutants. The Python suite passed 309 tests in Ghosts Co-op workflow run 45257107588; all five jobs passed. Phase 32 added the GSC call-site/callback graph analyzer and six synthetic tests; Phase 33 added the player-state ownership audit and five synthetic tests. Phase 34 adds the Ghidra split-screen builtin XREF exporter and Campaign/Multiplayer caller-fingerprint comparer (ten synthetic tests). Phase 35 integrates that XREF export into the atomic headless Ghidra runner and pins the XREF CSV hashes in the existing manifest. Phase 36 extends the same run to produce the combined Campaign/Multiplayer builtin-XREF comparison JSON as a sixth atomic output. Phase 37 adds an optional Ghidra headless pass to the TU 1.16 capture analyzer, producing the PPU evidence bundle in the same atomic capture run. Phase 38 hardens the builtin-XREF analysis: Ghidra exports explicit call/data reference flags, only actual call references can produce Multiplayer-only call-path leads, and CSV output is UTF-8. Phase 39 makes no-reference rows explicitly false/false and standardizes UTF-8 output across both PPU inventory and XREF exporters. Phase 40 distinguishes direct from computed call references, ranks direct-call evidence first, and records both counts in the PPU and capture manifests. Phase 41 bounds optional OPD seeding to the unsigned 32-bit PPU address space and rejects overflowing ranges before Ghidra reads memory. Phase 42 validates 8-byte OPD table alignment and 4-byte PPU instruction-pointer alignment before function seeding. Phase 43 makes OPD seeding fail closed and requires a structured JSON result, reconciled function counts, and a SHA-256 summary hash in the manifest. Phase 44 upgrades the OPD report to schema 2 with exact slot, empty/non-empty/unreadable descriptor and valid/invalid code-pointer accounting; the runner reconciles every slot against the requested OPD size before promoting evidence. Phase 45 extends the read-only ELF inspector with full OPD slot, TOC-mapping, alignment and trailing-byte diagnostics, and emits an operator seed-range candidate only when the section is structurally valid. Phase 46 lets the Ghidra runner consume that report directly, verifies its SHA-256 against the exact ELF, records report provenance, and rejects ELF inputs that change during analysis. Phase 47 independently reparses the exact ELF with the same read-only PPU inspector and requires the report's complete OPD structure to match the recomputed result, preventing edited JSON claims from selecting a different range. Phase 48 adds per-function OPD descriptor and TOC mapping metadata to the PPU inventory so Multiplayer-only candidates carry their descriptor addresses and TOC mapping status into the comparison report. Phase 49 distinguishes exact OPD code-pointer-to-function-entry matches from pointers merely contained inside a function, accounts for unresolved executable pointers, and reconciles the exporter report against both the OPD seeder and per-function CSV before promotion. Phase 50 records each mapped TOC pointer's Ghidra memory-block name, read/write/execute permissions and offset within that block, with strict JSON validation in the inventory comparer. Phase 51 adds loaded, initialized, artificial, external and volatile block flags so analysis-only or uninitialized memory cannot be mistaken for runtime-backed TOC data. Phase 52 compares normalized TOC-region signatures for matching Campaign/Multiplayer function fingerprints, excluding absolute addresses while retaining block name, relative offset, permissions and provenance. Phase 53 adds a read-only local verifier for the exact BLUS31270 disc/update SFO and installed TU 1.16 EBOOT/default.self/default_mp.self hashes, and reports whether decrypted PPU ELFs are available without copying or uploading game binaries. Phase 54 adds a one-command orchestrator that reuses the verified preflight, derives hash-bound OPD ranges from both decrypted ELFs, and invokes the Ghidra inventory/comparison runner without manual address guesses. Phase 53 preserves TOC-region signature multiplicity across duplicate fingerprint groups instead of collapsing repeated signatures, and reports per-signature cross-build counts. Phase 54 preserves resolved call-graph edge multiplicity across duplicate caller fingerprints and reports callee-fingerprint edge counts even when the unique target sets are identical. Phase 55 counts distinct resolved callee entries that share a fingerprint while deduplicating repeated references to the same entry. Phase 56 fixes the decrypted-ELF verifier's program-header lookup and tests the real PPU parser's native-readiness gate. Phase 57 expands the three uploaded mission audits to 68 exact file/function hotspots and prevents same-name functions in unrelated scripts from being mixed. Phase 58 makes the one-command native pipeline check all six inventory artifacts before writing preflight files, preventing partial output folders when an earlier inventory already exists. Phase 59 adds an end-to-end synthetic ELF test that runs the real PPU inspector, derives both OPD ranges and verifies the hash-bound handoff to the Ghidra runner. Phase 60 inventories every playerpiggyback/player-array and secondary-client reference across all three mission GSC sets, not only the 68 high-reference functions. Phase 61 adds a read-only FastFile identity verifier and confirms the three attached mission FastFiles match the exact Phase 31 extraction hashes, sizes and IWff0100 headers. Phase 62 validates the explicit big-endian FastFile version field as well as magic, size and SHA-256. Phase 63 hashes each mission FastFile and reads its header through one open file handle, then verifies descriptor and path metadata stayed stable to reject in-place mutation or path replacement during verification. Phase 64 feeds exact file/function targets from the all-mission player-container inventory into the GSC call graph, reporting incoming call sites and callback-aware reachability without substituting same-name functions from other ScriptFiles. Phase 65 adds one read-only runner that executes the ownership and exact-target call-graph audits together, pins a SHA-256 manifest of every input GSC file, and rejects source changes during analysis. Phase 66 prevents the combined report path from resolving to any source GSC file, even when overwrite is explicitly enabled. Phase 67 gates the one-command player-container audit on all three pinned mission FastFile identities, rechecks them after analysis and requires a one-to-one mapping of the three GSC source directories. The six-file evidence bundle and atomic comparison output passed all CI jobs. The pinned GSC Tool job compiled, decompiled and byte-identically recompiled the native Campaign local-session bridge (2,606 bytes; SHA-256 `b0dac7ddb682fdf2836e2e18343b04068e5f57944dbf3f367e736104dab9f15c`) and both synthetic Clockwork NVG and AI-target helper fixtures. The bridge source SHA-256 is `81126c094e095adc1a462f8ff0e50a220a3ae866ca9c71d80b092e7a6be1fa61`. This is source/build validation, not proof of live PS3/RPCS3 Campaign Player 2 creation, per-viewport NVG rendering or live AI aggro. This is MODELLED contract coverage, not proof that the real game runtime can spawn or render a second Campaign player. The researched test pyramid and real-device gates are in docs/testing-strategy.md; the Phase 1 success gate is defined in docs/local-coop-first.md. Phase 68 records the actual uploaded FastFiles and Phase 31 GSC extraction: all three file hashes match, 249 scripts and 4,304 functions are scanned, and 13 exact player-container functions produce 14 incoming call sites. Phase 69 adds a source scanner for global assignments, array mutations and per-entity writes to playerpiggyback/playengineeffects, separating script-side initialization from ordinary reads. Phase 70 traces client-scoped visibility calls, per-client FOV DVAR slots and the playengineeffects comparison to identify the strongest second-client candidates.

## Important

No retail game files, copyrighted game assets, original EBOOT or installable game PKG are included. The project uses public technical research and will require exact executable identity and runtime validation before enabling any game-memory hooks.

Phase 53 adds an offline local-input verifier for the exact BLUS31270 TU 1.16 disc/update SFO and installed EBOOT/default.self/default_mp.self hashes. It never decrypts or uploads game files; it reports whether matching decrypted Campaign and Multiplayer PPU ELFs are available for the next analysis gate.

Phase 71 extends the Ghidra PPU string/XREF search with playerpiggyback, playengineeffects, per-client FOV DVAR names and client visibility builtins discovered in the uploaded Campaign scripts.


Phases 72–75 harden the native local-player readiness gate. Phase 72 added explicit fresh-input, same-frame, independent command-buffer, camera/view and HUD requirements; Phase 73 rejects malformed non-boolean evidence flags; Phase 74 requires a valid engine observation-frame identifier; and Phase 75 requires each input sample's frame number to match that observation frame. The C gate has 18 host test functions, and the latest Ghosts Co-op workflow run 45482944832 passed all five jobs. These are fail-closed contract/build results only: the gate is not connected to a verified Campaign frame hook, no exact TU 1.16 PPU capture pair has been analyzed, and no playable Campaign co-op build is claimed.


### Static TU 1.16 intake + PS3MAPI capture hardening (Phases 76–77)

- **Phase 76:** verified the user-owned BLUS31270 PARAM(2).SFO, EBOOT, Campaign default.self, and Multiplayer default_mp.self by exact SHA-256, file size, SELF key revision, entry point, ELF64/BE/PowerPC64 identity, and PT_LOAD map. The result is PASS_STATIC_INPUTS; encrypted segments were not decrypted.
- **Phase 77:** hardened both manual read-only PS3MAPI dump scripts. Each SEG file is now checked after rename, and the DUMP_DONE marker is reached only after both final segment files exist. Added regression checks for the required command order; the five local PS3MAPI script tests pass.
- The latest available Library inventory still has no Campaign/Multiplayer SEG0/SEG1 runtime dumps or Pad Probe v4 logs. Therefore no runtime ELF, Ghidra PPU inventory, native Campaign Local Client initializer, or playable Player 2 has been demonstrated.
- Reports: reports/PHASE76_REPORT_AR.md, reports/PHASE77_REPORT_AR.md; machine-readable summaries: reports/PHASE76_INPUT_VERIFICATION.json, reports/PHASE77_CAPTURE_SCRIPT_SUMMARY.json.


### Phase 78 — packaged TU 1.16 capture handoff

- Assembled a seven-file capture kit containing the two current manual PS3MAPI dump scripts, the verified Pad Probe v4 fake-signed PRX, separate Campaign/Multiplayer mode files, and Arabic instructions.
- Verified the Pad Probe artifact ZIP SHA-256 against GitHub Actions run 45482944832. Its source files are unchanged at the current branch head. Verified both dump-script Git blob hashes against the current branch.
- ZIP integrity and all seven internal SHA-256 entries pass. Phase 76 and Phase 77 targeted suites each pass 5/5 locally.
- The ZIP is a local handoff artifact, not committed to the repository and not a playable mod. No runtime capture has been received.
- Report: reports/PHASE78_REPORT_AR.md; summary: reports/PHASE78_CAPTURE_KIT_SUMMARY.json.


### Phase 79 — PS3MAPI process provenance

- Added instructions to record PROCESS GETCURRENTPID and PROCESS GETNAME for each manually selected mode, plus a capture metadata template.
- Process name is diagnostic metadata only; the workflow does not assume it automatically distinguishes Campaign from Multiplayer.
- PS3MAPI script suite: 6/6 local tests pass. No memory-write command or executable address changed.
- Report: reports/PHASE79_REPORT_AR.md; summary: reports/PHASE79_CAPTURE_METADATA_SUMMARY.json.


The Phase 79 handoff package is Ghosts_TU116_Capture_Kit_Phase79.zip (8 payload files + manifest (9 ZIP entries); SHA-256 11d8efcce114a5c422c3ac9f57eca774acdf011e9b4d6d886db3afbf190fac08). It includes the PROCESS GETNAME metadata step and capture template. The binary remains a local handoff artifact; no original game files are included.


### Phase 80 — strict capture metadata intake

- Added `tools/ps3mapi/verify_capture_metadata.py` to parse the operator's per-mode metadata with a strict key schema, reject duplicate/unknown/missing fields, validate BLUS31270/TU 1.16, the selected mode, local timestamp format, positive PID, and a non-empty bounded process name.
- The verifier binds the declared SEG and Pad Probe filenames to the expected Campaign/Multiplayer names and checks that the actual SEG files exist in the corresponding dump directory. When a log is supplied, its basename must match the metadata.
- Integrated paired `--campaign-metadata` / `--multiplayer-metadata` options into `analyze_tu116_runtime_capture.py`. A single metadata file is rejected; validated metadata hashes and recorded process details are included in `capture_summary.json`.
- Same PID or same process name across the two captures is allowed. PROCESS GETNAME remains diagnostic metadata, not mode detection or proof that a PID belonged to the selected mode.
- Added direct validator and analyzer integration tests. This remains host-side intake validation; no physical PS3 runtime capture or Campaign Player 2 is claimed.
- Report: `reports/PHASE80_REPORT_AR.md`; summary: `reports/PHASE80_CAPTURE_METADATA_VALIDATION.json`.


### Phase 81 — reject the bot/test-client path as local Player 2

- Compared the public IW6 Windows x64 dedicated-server references labelled iw6_ds_ps4 and iw6_ds_xb3 at lizardpeter/ghosts-decomp commit 513f0b6bdf0de32d16df3af914b26df01ee04ec9. The repository README explicitly identifies these as dedicated-server builds, not retail console clients.
- In those server binaries, SV_AddTestClient creates an NA_BOT connection through svs.botport and SV_DirectConnect, then marks the slot as a test client. GScr_AddTestClient and GScr_SpawnTestClient are wrappers for that bot/test-client path; they are not physical local-controller registration.
- CL_DoesControllerIndexHaveALocalClientNum and PlayerCmd_IsSplitscreenPlayer observations from these server binaries are not transferable to retail PS3/PS4/Xbox client split-screen behavior.
- Rejected SV_AddTestClient/SV_SpawnTestClient as a local Player 2 implementation. Added a regression test that prevents the Campaign local-session bridge from calling spawnTestClient/addTestClient/addBot or their native names.
- All listed addresses are Windows x64 dedicated-server reference addresses for PS4/Xbox-hosted server builds, not retail console-client addresses or PS3 PPU addresses. No such offsets are copied into the PS3 PRX.
- Report: reports/PHASE81_REPORT_AR.md; machine-readable summary: reports/PHASE81_CROSS_PLATFORM_REFERENCE_AUDIT.json.
- **PS3-specific proof remains blocked** on Campaign/Multiplayer SEG0/SEG1 runtime captures and Pad Probe logs.


### Phase 82 — independent DualShock input telemetry

- Added a separate read-only `pad_input_probe` PRX that reuses `ghc_local_input_poll_ps3` to call `cellPadGetInfo2` and `cellPadGetData` for ports 0 and 1. It logs raw digital buttons, mapped button masks, pressed/released edges, raw/normalized sticks and L2/R2 pressure for 30 one-second samples.
- Campaign and Multiplayer logs are separate; mode remains manually selected. The probe does not call `cellPadInit`, `cellPadEnd`, or `cellPadSet*`, and does not patch game memory or create a local client.
- Added host-mock tests for the PS3 adapter and `tools/compare_pad_input_probe_logs.py` to require simultaneous port visibility, successful reads, button activity on both ports, and distinct button states in the same sample in both modes.
- CI run #1437: 452 Python tests passed, PS3 adapter mock passed, PS3DK PRX build and `prx-gen check` passed, 6/6 jobs successful.
- Report: `reports/PHASE82_REPORT_AR.md`; machine-readable summary: `reports/PHASE82_DUALSHOCK_INPUT_PROBE.json`.
- **Hardware gate remains:** no physical PS3 run is claimed. Input telemetry must be captured in Campaign Offline and Multiplayer Offline before native PS3 Local Client analysis can proceed.


### Phase 83 — DualShock input capture integrity

- The input-log comparator now recomputes the mapped button mask from raw `digital1`/`digital2` using the exact portable decoder mapping. It rejects out-of-range fields, impossible connected/read-error combinations, and inconsistent button masks.
- It replays per-port `pressed`/`released` edges against the previous successful sample; transient read failures do not advance the expected state. It also recomputes normalized stick axes from raw axis values using the v1 default deadzone (20), rejecting mismatches.
- Strong button independence now requires each controller to hold at least one exclusive button in the same simultaneous sample; a merely different overlapping mask is not enough. Stick-axis and L2/R2 activity/independence are reported separately as supporting evidence.
- The runtime analyzer now always writes `input_probe_comparison.json`, including an explicit `NOT_PROVIDED` state when logs are absent, and includes per-mode independent button/axis/trigger sample indices in `capture_summary.json`.
- CI run [#1460](https://github.com/Vikik0/PS3-SCALE/actions/runs/45504502823): **6/6 jobs successful**, including **467 Python tests**, PS3 adapter/PRX builds and `prx-gen check`.
- Report: `reports/PHASE83_REPORT_AR.md`; machine-readable summary: `reports/PHASE83_INPUT_VALIDATION.json`.
- **Hardware gate unchanged:** no physical PS3 logs are claimed. Input telemetry does not prove a native second local client, player entity, camera/HUD, or playable Campaign co-op.


### Phase 84 — mission event ownership and duplication-risk audit

- Re-reviewed nine high-priority routines from the supplied Black Ice, Carrier and Cornered GSC archive: finale death, ascend, flare-stack console, Sparrow knockback, rope cut, victory landing, gun-down weapon hide, building fall, and virus upload.
- The audit separates per-player field writes from level field writes and records mission failure, autosave, slow-motion, world-physics, level-wide event and animation-rig side effects.
- Confirmed existing multi-player script patterns: Carrier's monitorscavengerpickup iterates level.playerpiggyback and creates per-player HUD/FOV state while also writing a shared shadow-center dvar; Carrier's get_different_favoriteenemy selects the other existing player; Cornered stealth scripts iterate all existing players; Black Ice's scuba branch still only runs its player routine for the primary player.
- Engineering rule: one mission-event coordinator plus explicit per-player handlers. Never duplicate mission flags, autosave, mission failure, slow motion, world physics, shared objectives or devices. Shared rope/console/weapon-hide/virus-upload interactions need a single-owner lock.
- Keep Logan as Player 1 and Hesh as an NPC. Do not globally replace level.player, and do not mistake level.playerpiggyback or level.playengineeffects for creation of a native second Local Client.
- CI run [#1470](https://github.com/Vikik0/PS3-SCALE/actions/runs/45505931126): **6/6 jobs successful**, **469 tests passed**.
- Report: reports/PHASE84_REPORT_AR.md; machine-readable audit: reports/PHASE84_MISSION_EVENT_OWNERSHIP.json.
- **Implementation gate:** this is static GSC analysis only. No FastFile was patched. Native PS3 Local Client 1 and physical Campaign/Multiplayer input evidence are still unverified.


### Phase 85 — reusable Campaign event ownership lock

- Added a 16-ID GSC event-lock API inside the Local Session bridge. Each event has one owner; each local player can own one active event; different event IDs can run concurrently for different players.
- Acquire/release validate the current native player pair. Release is owner-only; completion is recorded only on successful release; abort releases without marking complete. Completed event state survives session rebind, while active locks are cleared before stale player aliases are removed.
- The API fails closed before initialization and has no wait/yield inside acquire/release/abort critical sections.
- It is lifecycle-integrated with the session bridge but is **not yet wired into the original Black Ice/Carrier/Cornered mission handlers**. Those handlers still require a real native second local player before integration.
- GSC Tool 1.6.2.364 compile/decompile round-trip passed; the 3,680-byte GSCBIN source and bytecode hashes are pinned in the manifest.
- CI run [#1494](https://github.com/Vikik0/PS3-SCALE/actions/runs/45508312714): **6/6 jobs successful**, **480 tests passed**, PS3 PRX and both read-only probes built and validated.
- Report: reports/PHASE85_REPORT_AR.md; API summary: reports/PHASE85_EVENT_LOCK_API.json.
- **Runtime gate unchanged:** the bridge is compiled but not injected or tested on a physical PS3. It does not create a Local Client or prove playable Campaign co-op.


### Phase 86 — capture-kit integrity validator

- Added tools/verify_capture_kit.py to validate the kit target (BLUS31270/TU 01.16), required probe/bootstrap/dump files, all manifest sizes and SHA-256 hashes, safe relative paths, both GSC source/GSCBIN manifests, and separation of experimental GSCBIN files from the PS3 payload directory.
- Added seven tests for valid packages, tampering, missing files, path traversal, wrong TU, GSC source mismatch and misplaced GSCBIN.
- Ran the validator against the actual Phase 85 package: **29/29 files verified**, both GSC manifests verified.
- CI run [#1499](https://github.com/Vikik0/PS3-SCALE/actions/runs/45509141329): **6/6 jobs successful**, **487 tests passed**.
- Report: reports/PHASE86_REPORT_AR.md; JSON: reports/PHASE86_CAPTURE_KIT_VALIDATION.json.
- The validator proves file integrity only. It does not prove PS3 runtime compatibility or playable Campaign co-op.


### Phase 87 — reproducible CI capture-kit packaging

- Added tools/build_capture_kit.py and a package-capture-kit job that waits for all six build/test jobs, downloads the five PRX/GSC artifacts, assembles the PS3 and Experimental_GSC_Not_Installed directories, generates per-file SHA-256 manifests and runs verify_capture_kit.py before upload.
- The builder fails closed on missing artifacts, missing dump scripts, bad GSC source/bytecode pins or invalid package layout.
- CI run [#1511](https://github.com/Vikik0/PS3-SCALE/actions/runs/45510341497): **7/7 jobs successful**, **489 tests passed**.
- Uploaded artifact: ghosts-tu116-development-capture-kit (ID 14974903362). Inner ZIP: 483,396 bytes; 46/46 files verified; both GSC manifests verified.
- SHA-256: 477f6b84992369b78972cff16f7b57ed4df3d8cf4b395ed1f934ce162f44ea35.
- Report: reports/PHASE87_REPORT_AR.md; JSON: reports/PHASE87_CAPTURE_KIT_CI.json.
- The package is a reproducible development/capture kit, not a playable co-op mod. Hardware and native Local Client validation remain outstanding.


### Phase 88 — mission event lock ownership map

- Mapped nine critical handlers across Black Ice, Carrier and Cornered to either four player-owned shared interactions (stable Event IDs 0–3) or five mission-coordinator-only events.
- Shared interactions: Black Ice flare-stack console (0), Carrier rope cut (1), Cornered weapon hide (2), and Cornered virus upload (3).
- Finale death, ascend choreography, Sparrow knockback/slow motion, victory landing/autosave and building collapse remain single-shot mission events; they must not be run once per player.
- Added `tools/verify_mission_event_lock_map.py` and negative tests for duplicate/out-of-range IDs, wrong policy, missing handlers and missing lock API.
- Report: `reports/PHASE88_REPORT_AR.md`; map: `reports/PHASE88_MISSION_EVENT_LOCK_MAP.json`.
- **Status:** ownership map only. Original mission handlers are not wired; no FastFile injection or physical PS3 test is claimed.

### Phase 89 — original trigger/activator lineage audit

- Audited the actual user-supplied Phase 31 mission archive and pinned the source hashes for five trigger/helper GSC files.
- Traced all four shared interactions to their call sites and trigger helpers. None currently propagates a validated player activator: Black Ice and Cornered helpers poll `level.player`; Carrier's `var_0` is the rope entity, not the player; Cornered upload handoffs use global flags.
- Result: **0/4** shared interactions are safe to wire to Event Lock yet. Applying the lock around `level.player` would only serialize Logan and would not provide Player 2 support.
- Confirmed the five mission-coordinator events remain single-shot and have no player Event Lock ID.
- Added `tools/mission_event_activator_audit.py`, eight tests, and a package-level validator for the pinned report.
- Report: `reports/PHASE89_REPORT_AR.md`; JSON: `reports/PHASE89_MISSION_EVENT_ACTIVATOR_AUDIT.json`.
- CI run [#1535](https://github.com/Vikik0/PS3-SCALE/actions/runs/45590863539): **7/7 jobs successful**, **508 tests passed**.
- Capture-kit artifact ID: `15011056497`; inner ZIP: **523,270 bytes**, **55/55 files verified**, both GSC manifests and the activator audit verified.
- Inner ZIP SHA-256: `853c3958d9601b7e63b6d7ef5d0fb2e42b6c5fb6f4cb878792d70c5fc655a702`.
- **Runtime gate remains:** no original mission GSC was changed, no FastFile was rebuilt/injected, and no physical PS3 runtime result is claimed.


### Phase 90 — mission event refactor dependency graph

- Built a source-pinned call/dependency graph for the four shared interactions: **4 chains, 29 functions and 31 call edges**.
- Carrier entity ownership is now explicit: `ranktable` runs on the AI rappeller; `var_0` is the map struct/animation target; `self` remains the rope animation entity. The real player must be a separate argument at the per-player rope watcher boundary.
- Counted generic trigger-helper reuse: `black_ice_util::waittill_trigger` has 3 call sites; `player_scripted_anim_util::waittill_trigger` has 15. Do not rewrite either helper globally; add a separate player-aware helper and migrate only intended call sites.
- Mapped per-player handlers versus shared state for the flare-stack console, rope cut, weapon hide and virus upload. Five mission-coordinator events remain single-shot.
- Added graph validation to the capture-kit verifier and four negative tests for escaped edges, entity confusion and coordinator-lock misuse.
- Report: `reports/PHASE90_REPORT_AR.md`; graph: `reports/PHASE90_MISSION_EVENT_REFACTOR_GRAPH.json`.
- CI run [#1554](https://github.com/Vikik0/PS3-SCALE/actions/runs/45593854111): **7/7 jobs successful**, **513 tests passed**.
- Capture-kit artifact ID: `15011738053`; inner ZIP: **534,872 bytes**, **57/57 files verified**, both GSC manifests and both mission-event audit reports verified.
- Inner ZIP SHA-256: `d854b621df9e4ae12a1e96cc3c46bce78928ae4c6d7c7c1eceed67f06b3857af`.
- **Implementation remains gated:** native Local Client 1 is not proven; no original mission GSC was modified and no FastFile was rebuilt/injected.


### Phase 91 — portable event-lock contract model

- Added a host-only C model of the existing GSC event-lock API: 16 event IDs, two player slots, one owner per event, and one active event per player.
- The model checks owner-only release, completion versus cancellation, abort on player/session loss, stale-pair cleanup, idempotent initialization, completed-state preservation and explicit event reset.
- Added an invariant checker for owner/active-slot consistency, **21 C test cases**, and eight Python source-contract tests against the actual GSC bridge and Phase 88 event-ID map.
- The C model is test-only and is **not linked into the PS3 PRX**. The original mission GSC files and FastFiles remain unchanged.
- CI run [#1558](https://github.com/Vikik0/PS3-SCALE/actions/runs/45715817740): **7/7 jobs successful**, **522 Python tests passed**, **21/21 event-lock C cases passed**. GSC Tool compile/decompile, PS3 CEX bootstrap and both read-only controller probes passed.
- Capture-kit artifact ID: `15069646947`; inner ZIP: **535,221 bytes**, **57/57 files verified**.
- Inner ZIP SHA-256: `76ca5d4daa7eae63ea54ca08ef6543a451909d945362d7b4e4558f8052199499`.
- Report: `reports/PHASE91_REPORT_AR.md`; contract: `docs/phase91-event-lock-contract.md`; JSON: `reports/PHASE91_EVENT_LOCK_MODEL.json`.
- **Native gate unchanged:** 0/4 shared mission interactions are ready for wiring until the original trigger path supplies a validated activator and the exact TU 1.16 native second Local Client is proven. No original mission GSC was patched, no FastFile was rebuilt/injected, and no physical PS3 Campaign co-op is claimed.


### Phase 92 — player-aware trigger adapter fixture

- Added a compile-only GSC fixture for a dedicated trigger adapter. It captures the actual `activator` from the supplied trigger entity, validates the native pair again after the wait, verifies that the activator is one of the registered local players, and acquires the Event Lock for that exact entity.
- Added explicit-player release/abort wrappers and six Python source-contract tests. The fixture never substitutes `level.player` and does not rewrite the generic trigger helpers used by unrelated mission call sites.
- GSC Tool 1.6.2 compile/decompile/recompile passed with byte-for-byte output. Local bridge stubs exist only to compile the isolated fixture; they are not part of the PS3 payload.
- CI run [#1561](https://github.com/Vikik0/PS3-SCALE/actions/runs/45716590387): **7/7 jobs successful**, **528 Python tests passed**, **21/21 event-lock C cases passed**, PS3 bootstrap/probes and capture-kit verification successful.
- Capture-kit artifact ID: `15070815301`; inner ZIP: **535,224 bytes**, **57/57 files verified**.
- Inner ZIP SHA-256: `89acce060f71686385b160709cda6ac47d5e44dac04999ab205c79645a15a196`.
- Report: `reports/PHASE92_REPORT_AR.md`; contract: `docs/phase92-player-aware-trigger-fixture.md`; JSON: `reports/PHASE92_TRIGGER_ADAPTER.json`.
- **Runtime gate unchanged:** compile success does not prove trigger activator semantics on retail IW6/PS3. The original mission readiness remains **0/4**; no original GSC was changed, no FastFile was rebuilt/injected, and no physical Campaign co-op is claimed.


### Phase 93 — preserve the original Campaign player array

- Fixed a session-invalidation hazard: `level.playerpiggyback` is an original Campaign player-entity array used by unrelated mission scripts, so the bridge must not clear it to `undefined`.
- Invalidation now rebuilds the array from current player entities, preserves the previous primary if still present, otherwise falls back to the first live player, and clears only stale secondary/bridge aliases.
- Added source-contract coverage for array preservation and invalidation ordering. Python suite: **529/529 passed**; event-lock C model: **21/21 passed**.
- Updated bridge manifest pins: source SHA-256 `91beae5278cddaa23521aabdfec134e9323f8b28a7f610b470709a4236da4e8b`; compiled GSCBIN SHA-256 `e88a92b314928c36ee55fbbcba1593dd3e0a6ed7d719bf75de4743e750a72ad9`; **3,800 bytes**.
- CI run [#1564](https://github.com/Vikik0/PS3-SCALE/actions/runs/45717668577): **7/7 jobs successful**, including GSC compile/decompile, PS3 CEX bootstrap, both read-only probes, and capture-kit packaging.
- Capture-kit artifact ID: `15070258524`; inner ZIP: **535,522 bytes**, **57/57 files verified**; SHA-256 `a62b25f7b2c201d6f2714872d6c916df1650e68fcf83f20537dc6d16d96f59f1`.
- Report: `reports/PHASE93_REPORT_AR.md`; contract: `docs/phase93-player-array-preservation.md`; JSON: `reports/PHASE93_PLAYER_ARRAY_PRESERVATION.json`.
- **Native gate unchanged:** second Local Client and trigger activator semantics remain unproven on the target PS3 runtime; original shared interactions remain **0/4 ready**. No original mission handler or FastFile was modified.


### Phase 94 — preserve the original player array during startup rebind

- Removed the startup `level.playerpiggyback = undefined` reset. Final binding now requires a fresh engine player-array query; if it is absent, status 2 is returned and stale snapshots are not bound.
- CI [#1567](https://github.com/Vikik0/PS3-SCALE/actions/runs/45719086601): **7/7 successful**, **530 Python tests passed**. Capture kit: 57/57 files, 535,722 bytes; artifact `15072135172`.

### Phase 95 — strict Event Lock completion input

- GSC release now rejects undefined completion values and accepts only numeric 0/1, matching the host C model.
- CI [#1569](https://github.com/Vikik0/PS3-SCALE/actions/runs/45719357177): **7/7 successful**, **531 Python tests**, **21/21 Event Lock C cases**. Capture kit: 57/57 files, 535,838 bytes; artifact `15070344311`.

### Phase 96 — PRX export/NID synchronization

- Added all ten read-only GSC wrapper-loader diagnostics to `exports.yml`. Corrected their NIDs to the values computed by PS3DK `nidgen`; YAML and assembly now match exactly.
- `nidgen` accepted **29/29 exports**. CI [#1571](https://github.com/Vikik0/PS3-SCALE/actions/runs/45719901826): **7/7 successful**, **533 Python tests**. Capture kit: 57/57 files, 536,522 bytes; artifact `15072500004`.

### Phase 97 — portable GSC wrapper parser

- Extracted wrapper parsing into a portable C module shared by PS3 file I/O and host tests. It checks magic, segment lengths, capacity, exact total size and FNV-1a.
- CI [#1575](https://github.com/Vikik0/PS3-SCALE/actions/runs/45720639607): **7/7 successful**, **536 Python tests**, **10/10 parser C tests**. Capture kit: 57/57 files, 536,971 bytes; artifact `15072465712`.

### Phase 98 — mocked PS3 GSC loader tests

- Added a mock Cell FS API and tests against the actual PRX loader: success, open/read errors, oversized and exact-capacity files, invalid wrappers and reload reset.
- CI [#1577](https://github.com/Vikik0/PS3-SCALE/actions/runs/45720980397): **7/7 successful**, **536 Python tests**, **10/10 parser tests**, **9/9 loader tests**. Capture kit: 57/57 files, 536,971 bytes; artifact `15071488495`.

### Phase 99 — partial reads and close failures

- The loader now accumulates short reads until EOF or capacity, probes for an extra byte, clears failed snapshots and reports `CLOSE_FAILED` when closing the file fails.
- CI [#1578](https://github.com/Vikik0/PS3-SCALE/actions/runs/45721292708): **7/7 successful**, **536 Python tests**, **10/10 parser tests**, **12/12 loader tests**. Capture kit: 57/57 files, 537,208 bytes; artifact `15072062945`.

**Native/runtime gate remains unchanged:** the second Campaign Local Client and trigger activator semantics are not proven on the target PS3. The original shared mission interactions remain **0/4 ready**. No original mission GSC or FastFile was modified, and no physical Campaign co-op result is claimed.


### Phase 100 — automatic native-pair rebind after disconnect

- The guarded session watcher now survives invalidation, polls the fresh engine player array every 250 ms, and rebinds only after strict primary/secondary validation. It never creates or clones a player. Added an explicit stop flag.
- CI [#1581](https://github.com/Vikik0/PS3-SCALE/actions/runs/45721984971): **7/7 successful**, **537 Python tests**, **21/21 Event Lock C tests**. Capture kit: 57/57 files, 537,428 bytes; artifact `15072377465`.

### Phase 101 — recovery watcher on initial bind failure

- `main()` now starts the recovery watcher even when no fresh player array exists or the first native-pair bind fails. A pair that appears later can be bound without another manual `main()` call.
- CI [#1583](https://github.com/Vikik0/PS3-SCALE/actions/runs/45722241399): **7/7 successful**, **538 Python tests**, **21/21 Event Lock C tests**, **10/10 parser C tests**, **12/12 loader C tests**. Capture kit: 57/57 files, 537,563 bytes; artifact `15072508057`.

**Runtime gate remains:** reconnect behavior has not been observed on the target PS3. The second native Campaign Local Client and the original four shared interactions remain unproven (**0/4 ready**).


### Phase 102 — prevent competing main/recovery monitor loops

- `main()` now returns without clearing aliases or starting another bind loop when the guarded recovery monitor is already active and not stopping.
- CI [#1586](https://github.com/Vikik0/PS3-SCALE/actions/runs/45722748409): **7/7 successful**, **539 Python tests**, **21/21 Event Lock C tests**, **10/10 parser tests**, **12/12 loader tests**. Capture kit: 57/57 files, 537,728 bytes; artifact `15073380575`.

**Runtime gate unchanged:** the second native Campaign Local Client and reconnect behavior remain unproven on the target PS3; the four original shared interactions remain **0/4 ready**.
