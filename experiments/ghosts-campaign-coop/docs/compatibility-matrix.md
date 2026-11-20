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
