# Compatibility matrix

## First hardware target

- Console firmware: **CEX 4.93**
- Game title update: **1.16**
- Players: **2 maximum**
- Player 1 remains Logan; Player 2 is an additional Multiplayer-style soldier actor.
- Story NPCs (including Hesh) must remain untouched.

This is a development target, not a verified playable build.

## Known PS3 title IDs

| Title ID | Region | Distribution | Profile status |
|---|---|---|---|
| BLUS31270 | US | Disc | Registered; untested |
| NPUB31301 | US | Digital | Registered; untested |
| NPUB50021 | US | Digital Hardened Edition | Registered; untested |
| BLES01945 | EU/AU | Disc | Registered; untested |
| BLES01948 | EU/AU | Disc | Registered; untested |
| NPEB01832 | EU | Digital | Registered; untested |
| NPEB01835 | EU | Digital | Registered; untested |
| BLJM61125 | JP | Disc | Registered; untested |
| BLJM61126 | JP | Disc | Registered; untested |
| NPJB00528 | JP | Digital | Registered; untested |

The list is a known-title-ID registry, not a promise that one binary supports every region. Each title ID must be checked against its own game files, update package, executable layout and memory map.

## Verified TU 1.16 update-package metadata

The public PS3 update catalogue lists these region-matched 1.16 package records. The package must match the installed game's Title ID; do not install a different region's update.

| Game Title ID | TU 1.16 update package filename | Listed size |
|---|---|---:|
| BLUS31270 | UP0002-BLUS31270_00-CALLDUTYGHOSTTU1-A0116-V0100-PE.pkg | 558,341,392 bytes |
| NPUB31301 | UP0002-NPUB31301_00-CALLOFDUTYGHOSTS-A0116-V0100-PE.pkg | 558,342,064 bytes |
| BLES01945 | EP0002-BLES01945_00-CALLDUTYGHOSTTU1-A0116-V0100-PE.pkg | 569,047,776 bytes |
| BLES01948 | EP0002-BLES01948_00-CALLDUTYGHOSTTU1-A0116-V0100-PE.pkg | 537,032,384 bytes |
| NPEB01832 | EP0002-NPEB01832_00-CALLDUTYGHOSTSDL-A0116-V0100-PE.pkg | 569,048,448 bytes |
| NPEB01835 | EP0002-NPEB01835_00-CALLDUTYGHOSTSDL-A0116-V0100-PE.pkg | 537,033,056 bytes |

Source catalogue: https://ps3.aldostools.org/updates.html

These records identify update packages only; they are not the full retail game. The full game must be a legitimately obtained copy, and its Title ID must match the update. Do not download or install anything until the final build's required profile is confirmed.

## Firmware policy

| Firmware family | Status | Policy |
|---|---|---|
| CEX 4.93 | First target | Build and test this first |
| Other CEX versions | Not verified | Add only after compile and hardware tests |
| DEX | Not targeted yet | Separate DEX build; never reuse CEX EBOOT/SPRX blindly |
| HEN | Not targeted yet | Separate loader/build validation required |

## Update policy

The first research target is **Title Update 1.16**. Do not assume a 1.16 executable/plugin is compatible with older title updates. A mismatch can cause a freeze or black screen.

## Compatibility gates

A profile is not marked supported until it passes:

1. Executable and update identity check.
2. Client/plugin load test on the matching console firmware.
3. Offline actor-spawn test in one Campaign mission.
4. Camera/input isolation test between Logan and Player 2.
5. Checkpoint and mission-transition test.
6. At least 20 minutes without a crash, followed by repeat testing.

No game files are bundled in this repository. The project will use metadata and user-owned game files for validation.
