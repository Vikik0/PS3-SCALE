# Candidate TU 1.16 spawn primitives

This is a reverse-engineering lead list, **not an executable patch**. The addresses below are reported in a public Ghosts PS3 TU 1.16 multiplayer offsets thread. They must be verified against the exact executable, region, update package, CEX build and runtime context before use.

| Candidate | Reported address | Why it matters | Confidence for Campaign |
|---|---:|---|---|
| G_Spawn | 0x290E58 | Allocates a game entity in the reported multiplayer context | Unknown |
| G_CallSpawnEntity | 0x28AB00 | Script/entity spawn dispatcher | Unknown |
| G_SetModel | 0x28FA4C | Assigns a model to an entity | Unknown |
| SP_Script_Model | 0x286710 | Script model spawn helper | Unknown |
| SV_LinkEntity | 0x32C420 | Links an entity into the world | Unknown |
| SV_UnlinkEntity | 0x32C3A0 | Removes an entity from world linking | Unknown |
| PlayerCMD_Spawn | 0x23D71C | Player script command; could be relevant to player creation | Unknown; VM context likely required |
| PlayerCMD_ClonePlayer | 0x23D850 | Creates a player clone in the researched IW6 build; not a full local client | Not a Player 2 creation path |
| PlayerCMD_IsSplitScreenPlayer | 0x23EA54 | Script query for split-screen identity | Unknown on PS3 Campaign; do not treat name as proof |
| PlayerCMD_IsSplitScreenPlayerPrimary | 0x23EB38 | Script query for primary split-screen identity | Unknown on PS3 Campaign; do not treat name as proof |
| PlayerCMD_SetOrigin | 0x23585C | Script command for actor position | Unknown |
| PlayerCMD_SetVelocity | 0x235958 | Script command for actor movement | Unknown |
| PlayerCMD_GiveWeapon | 0x233468 | Weapon assignment helper | Unknown |

## Reported data structures (not callable functions)

The same public PS3 TU 1.16 offset collection reports the following data addresses:

| Data | Reported address | Reported stride | Use in this project |
|---|---:|---:|---|
| `G_Client` base | `0xF44980` | `0x3700` bytes | Candidate client/player-state array; layout and Campaign validity unknown |
| `G_Entity` base | `0xE04980` | `0x280` bytes | Candidate entity array; layout and Campaign validity unknown |
| `scrVmPub_t` global | `0x13C3140` | — | Script VM state; not a safe standalone function-call context |

The ELF inspector now reports these as data candidates and maps addresses across both file-backed and zero-filled `PT_LOAD` memory. A BSS mapping has no file bytes to inspect; the tool will not fabricate them. These reported values remain unverified until matched against the exact EBOOT hash.

## Important decompilation finding — clone is not a second player

The public IW6 PS4 decompilation of `PlayerCmd_ClonePlayer` shows that the command:

- calls `G_SpawnPlayerClone`;
- copies the source player's client number, origin, angles and velocity into a new entity;
- marks the spawned entity as entity type 2;
- records it in the player-corpse tracking structure and copies character/model data.

That is a clone/visual actor path, not a second independently simulated local client. It does not show creation of a new `gclient_s`, a second user-command stream, or a new local viewport. Therefore, do not use `PlayerCMD_ClonePlayer` as the Player 2 implementation.

This is PS4 IW6 decompilation evidence, not a verified PS3 implementation. It is a strong reason to reject the clone command as the primary design, but the PS3 TU 1.16 executable still needs its own validation.

## Split-screen identity functions

The public PS3 TU 1.16 offset list names `PlayerCMD_IsSplitScreenPlayer` and `PlayerCMD_IsSplitScreenPlayerPrimary`. The public PS4 IW6 decompilation shows the first function returning false in that build, while the primary query inspects server-client state. The function names alone therefore do not establish that the PS3 functions identify two Campaign players correctly.

Treat these as script-command handlers, not documented general-purpose C APIs. They likely depend on a valid script VM/entity reference and must not be called directly as ordinary functions without confirming the calling convention and VM state.

## Important interpretation

The public thread is a multiplayer-oriented offset collection. It does **not** establish that these routines are safe or callable during Campaign.

- A spawned `G_Entity` is not automatically a human player, a local client, or a fully simulated actor.
- A clone command may create only a visual/corpse-like entity.
- A real second player needs a client/player-state lifecycle, its own input command stream, camera/view state and HUD ownership.
- The preferred Phase 1 route is to reuse Ghosts' existing local two-player initialization from an offline mode, if the exact PS3 build confirms it.
- These offsets are fallback research leads only. No candidate is approved for Campaign until verified against the exact executable and calling convention.

## Sources

- PS3 TU 1.16 offset list: https://nextgenupdate.com/forums/ghosts-mods-cheats-glitches/901435-ghosts-offsets-addresses-collection-3.html
- IW6 PS4 decompilation of `g_client_script_cmd_mp.cpp`: https://github.com/lizardpeter/ghosts-decomp/blob/main/analysis/iw6_ds_ps4/pseudocode/game_mp/g_client_script_cmd_mp.cpp.c
- IW6 PS4 decompilation of `sv_client_mp.cpp`, including split-screen address-state handling: https://github.com/lizardpeter/ghosts-decomp/blob/main/analysis/iw6_ds_ps4/pseudocode/server_mp/sv_client_mp.cpp.c
