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

The public thread is a multiplayer-oriented offset collection. It does **not** establish that these routines are safe or callable during Campaign. In particular:

- A spawned G_Entity is not automatically a human player, a network client, or a fully simulated AI actor.
- PlayerCMD_ClonePlayer may create only a clone/model or depend on an existing script VM player object.
- A Campaign script may have one-player assumptions for camera, input, HUD, checkpoints, triggers, cinematics and player state.
- The values may move between executable variants. Do not hard-code them into a CEX 4.93 plugin until the exact TU 1.16 executable is identified and checked.

## Safe proof-of-concept sequence

1. Work offline, preserve the original EBOOT, and identify the exact title ID and TU.
2. Verify candidate function addresses and calling conventions against that build; confirm the TOC and memory mapping.
3. First spawn and clean up a harmless inert model/prop in one Campaign test checkpoint.
4. Verify SV_UnlinkEntity/entity-free cleanup and repeat the spawn test to detect leaks.
5. Only then investigate a non-controllable soldier model or clone.
6. Test whether it can be made to follow a simple scripted path without taking over Logan's camera or input.
7. Do not attempt remote input, weapon replication or multiplayer until the offline actor remains stable through checkpoint reload and mission transition.

## Stop conditions

Abort the test if the game freezes, the entity pool stops allocating, mission triggers break, Logan's view/input changes, or the actor persists incorrectly after checkpoint reload. Do not use guessed offsets or functions from another region/build.

## Source

- TU 1.16 Ghosts offsets/functions collection: https://nextgenupdate.com/forums/ghosts-mods-cheats-glitches/901435-ghosts-offsets-addresses-collection-116updating-1.html

The forum values are third-party research and are not independently validated by this project.
