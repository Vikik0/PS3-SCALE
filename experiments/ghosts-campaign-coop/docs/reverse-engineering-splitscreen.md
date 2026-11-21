# Reverse-engineering notes: Ghosts local split-screen

## Target and confidence

The target is the PS3 CEX retail game at TU 1.16. Public PS3 offsets are useful starting points, but many are multiplayer-oriented and have not been verified against the exact EBOOT/profile used by this project.

A public IW6 PS4 decompilation is used only as a cross-platform engine reference. Its addresses, data layouts and observed behavior must not be copied directly into the PS3 runtime.

## IW6 script-level split-screen evidence

The public IW6 GSC dump exposes `isSplitScreen()`, `isSplitscreenPlayer()` and `isSplitscreenPlayerPrimary()` in its Multiplayer/Extinction scripts. The Extinction scripts also track connected player entities through `level.players` and contain a two-player down/revive flow. This narrows our search: find the engine builtins and the local-client initialization paths behind those script calls in the exact PS3 EBOOT. The dump is not a verified PS3 TU 1.16 Campaign source, and it does not include a Campaign script tree. Full findings and links: `docs/iw6-native-splitscreen-reference.md`.

## PS3 PPU ELF and OPD workflow

The project now includes `tools/ps3_elf_inspect.py`, a read-only ELF64 big-endian EM_PPC64 inspector. It maps the reported offsets against file-backed `PT_LOAD` segments, tests direct VA and explicitly supplied RVA+image-base interpretations separately, and inspects the section containing the ELF entry point as the OPD candidate region. It reads PPU OPD entries as big-endian `{u32 code, u32 TOC}` descriptors and reports the common module TOC. It never guesses the image base or modifies the binary.

`tools/ghidra/SeedGhostsOpdFunctions.java` can seed Ghidra function starts from the OPD range, while `tools/ghidra/FindGhostsSplitScreenBuiltins.java` searches for split-screen/player builtin names and reports cross-references. Import the exact decrypted ELF as `PowerPC:BE:64:default` and use only the OPD VA/size reported for that same file. The workflow is documented in `docs/ps3-elf-analysis.md`.

The inspector is covered by synthetic ELF unit tests. It has not yet been run against a Ghosts retail EBOOT, so none of the addresses below has been validated by this tool.

## Confirmed research leads

The public PS3 TU 1.16 offsets collection includes:

- `G_Client` base: `0xF44980`
- `G_Client` stride: `0x3700`
- `G_Entity` base: `0xE04980`
- `G_Entity` stride: `0x280`
- `scrVmPub_t` global: `0x13C3140`
- `PlayerCMD_Spawn`: `0x23D71C`
- `PlayerCMD_ClonePlayer`: `0x23D850`
- `PlayerCMD_IsSplitScreenPlayer`: `0x23EA54`
- `PlayerCMD_IsSplitScreenPlayerPrimary`: `0x23EB38`

These values are reported by a community offset list, not independently verified in this repository. They are not yet safe to dereference or call in Campaign.

## Cross-platform IW6 findings

The public PS4 decompilation of `PlayerCmd_ClonePlayer` shows a clone entity being allocated with `G_SpawnPlayerClone`, copying the source client's model/character information and transform, setting entity type 2, and registering the entity in player-corpse tracking. The function does not show a new `gclient_s`, a separate user-command stream, or a new local camera. This is not a complete second player.

The same PS4 decompilation shows `PlayerCmd_IsSplitscreenPlayer` returning false in that build. `PlayerCmd_IsSplitscreenPlayerPrimary` reads state from the server-client structure. This means the names of the PS3 script handlers are not enough to assume they provide a reliable Campaign player identity test.

The PS4 `SV_UpdateSplitscreenStateForAddr` routine compares connected client network addresses and updates per-client split-screen state during connection handling. This suggests the engine's split-screen model is tied to client/session lifecycle, not merely spawning a model. It is a research lead only; the PS3 implementation may differ.

The PS4 decompilation also shows a normal multiplayer client lifecycle involving `ClientConnect`, `ClientBegin`, `ClientSpawn`, `SV_ClientEnterWorld` and `SV_ClientThink`. A second controllable player must participate in an equivalent lifecycle or a verified local-client path. A visual clone cannot substitute for that.

The PS4/XB3 decompiled `client/splitscreen.cpp` is explicitly a single-local-client implementation: `CL_DoesControllerIndexHaveALocalClientNum` always returns `LOCAL_CLIENT_0`, `CL_ControllerIndexFromClientNum` returns one active controller index, and the public decomp header notes that this build has one local client and indexes the UI globals at slot 0. This is a platform/build limitation, not evidence about the PS3 retail game. It means the PC/PS4/XB3 client-side split-screen code cannot be used as a direct template for the PS3 local two-player path. The PS3 EBOOT must be analyzed independently.

## Consequences for this project

1. Do not implement Player 2 using `PlayerCMD_ClonePlayer`.
2. Do not call script-command handlers directly from C without a verified script VM invocation path.
3. Do not assume the public PS3 offsets match all regions, CEX/DEX executables or title-update states.
4. Identify the native local-player/session initialization path used by a supported two-player PS3 mode.
5. Compare the native two-player state with Campaign startup before writing any memory patch.
6. Only then design the Campaign bridge for Player 2's client/player state, controller command stream, camera and HUD.

## Input subsystem caution

The current PRX input poll uses `cellPadGetInfo2` and `cellPadGetData` without initializing or shutting down the pad subsystem. The game owns that lifecycle. A second call to `cellPadGetData` may interact with the title's own polling cadence, so the adapter is not considered safe for a frame hook until this is tested on the console. The portable frame tracker is host-tested separately and does not call the PS3 API.

## Sources

- PS3 TU 1.16 offsets collection: https://nextgenupdate.com/forums/ghosts-mods-cheats-glitches/901435-ghosts-offsets-addresses-collection-3.html
- IW6 PS4 decompilation, player script commands: https://github.com/lizardpeter/ghosts-decomp/blob/main/analysis/iw6_ds_ps4/pseudocode/game_mp/g_client_script_cmd_mp.cpp.c
- IW6 PS4 decompilation, server client lifecycle and split-screen state: https://github.com/lizardpeter/ghosts-decomp/blob/main/analysis/iw6_ds_ps4/pseudocode/server_mp/sv_client_mp.cpp.c
- IW6 PS4/XB3 decompilation, single-local-client split-screen stubs: https://github.com/lizardpeter/ghosts-decomp/blob/main/analysis/iw6_ds_ps4/pseudocode/client/splitscreen.cpp.c
- IW6 PS4/XB3 client header noting one local client: https://github.com/lizardpeter/ghosts-decomp/blob/main/include/client/client.h
