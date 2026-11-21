
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
| PlayerCMD_ClonePlayer | 0x23D850 | Player clone script command; worth investigating for a visual/actor clone | Unknown; not proof of a second controllable client |
| PlayerCMD_SetOrigin | 0x23585C | Script command for actor position | Unknown |
| PlayerCMD_SetVelocity | 0x235958 | Script command for actor movement | Unknown |
| PlayerCMD_GiveWeapon | 0x233468 | Weapon assignment helper | Unknown |

## Important interpretation

The public thread is a multiplayer-oriented offset collection. It does **not** establish that these routines are safe or callable during Campaign.

- A spawned G_Entity is not automatically a human player, a local client, or a fully simulated actor.
- PlayerCMD_ClonePlayer may create only a clone/model or depend on an existing script VM player object.
- The preferred Phase 1 route is to reuse Ghosts' existing local two-player initialization from an offline mode, if the exact PS3 build confirms it.
- These offsets are fallback research leads only; they are not the starting implementation.
- Campaign may have one-player assumptions for camera, input, HUD, checkpoints, triggers, cinematics and player state.
- Values may move between executable variants. Do not hard-code them into a CEX 4.93 plugin until the exact TU 1.16 executable is identified and checked.

## Safe proof-of-concept sequence

1. Establish a clean offline baseline with two controllers in a supported local mode on the target PS3.
2. Record which local-player slots, controller mappings, viewports and player-state structures are initialized.
3. Compare that runtime state with a one-player Campaign session.
4. Verify executable identity, function addresses, calling conventions, TOC and memory mapping before any hook.
5. Prefer enabling/reusing the native second local-player path over creating an arbitrary entity.
6. If an inert entity test is needed, spawn and clean up a harmless prop first; verify unlink/free behavior and repeat to detect leaks.
7. Only then investigate a soldier actor and the player-control pipeline.
8. Do not attempt online input or replication until the offline second player survives checkpoint reload and mission transition.

## Stop conditions

Abort if the game freezes, the entity pool stops allocating, mission triggers break, Logan's view/input changes, the second local player steals controller 1, or actors persist incorrectly after checkpoint reload. Never use guessed offsets or functions from another region/build.

## Source

- TU 1.16 Ghosts offsets/functions collection: https://nextgenupdate.com/forums/ghosts-mods-cheats-glitches/901435-ghosts-offsets-addresses-collection-116updating-1.html

The forum values are third-party multiplayer research and are not independently validated by this project.
