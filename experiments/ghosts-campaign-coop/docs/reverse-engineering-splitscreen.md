# Reverse-engineering notes: Ghosts local split-screen

## Target and confidence

The target is the PS3 CEX retail game at TU 1.16. Public PS3 offsets are useful starting points, but many are multiplayer-oriented and have not been verified against the exact EBOOT/profile used by this project.

A public IW6 PS4 decompilation is used only as a cross-platform engine reference. Its addresses, data layouts and observed behavior must not be copied directly into the PS3 runtime.

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
