# Crash Bash Online — PS3 development branch

This isolated branch is a build workspace for the Crash Bash project. The repository's `main` branch is not changed.

## Current milestone

- Use the open-source PSXonPS3 / PCSX-ReARMed PS3 front end as the emulator base.
- Give the PS1 core a virtual Multitap on controller port 1.
- Read four physical DualShock controllers on the PS3 and expose them as PS1 players 1–4.
- Build a separate PKG titled **Crash Bash Online**.

## Important status

This milestone is **four-player local foundation only**. It is not yet online multiplayer. The upstream PS3 front end currently reads two physical pads, although the PCSX-ReARMed core contains the PS1 Multitap protocol. The patch extends the front end to four pads and enables the existing `pcsx_rearmed_multitap = port 1` core option.

The online layer still needs frame synchronization, room/slot handling, disconnect recovery, and real two-console testing. Do not label a build as online until two separate PS3 consoles have completed an actual match.

## Game content

No Crash Bash disc image or BIOS is bundled. The user must create and supply their own lawful game dump. The emulator accepts the formats documented by upstream, including `.cue/.bin`, `.chd`, `.pbp`, and `.iso`.

For synchronized multiplayer, every console must use the exact same game image, emulator build, and BIOS configuration.

## Build

The workflow downloads the pinned upstream PSXonPS3 source, applies the project patch, builds a distinct PKG, validates its package header, and uploads the build artifact. The build requires the PS3 toolchain, PSL1GHT, and the shader compiler dependencies documented by upstream.

## Next milestone

Implement custom deterministic frame-lockstep networking over PS3 sockets. RetroArch's generic rollback Netplay path is not the chosen solution for PS1: Libretro's own FAQ says PSX Netplay is not practically suitable under its current rollback model. The project will use a PS1-specific lockstep path and verify it on hardware.
