# Crash Bash PS3 — PS1 execution base

The current online package uses the **RetroArch PSL1GHT frontend with PCSX-ReARMed statically linked as its PS1 core**. The PS3 XMB package starts RetroArch; it does not contain a native reimplementation of Crash Bash's original engine.

Online play in this package is provided by **RetroArch Netplay**. The PCSX-ReARMed core supplies PS1 emulation, serialization and Multitap support; RetroArch handles host/client synchronization, the public host list, NAT traversal and relay connections. The build configures Multitap on Port 1 and allows one host plus three remote clients.

The package does not redistribute Crash Bash disc data or a PlayStation BIOS. Each player must use a legally obtained BIN/CUE dump and should use the same game revision, BIOS choice and core options. A BIOS dumped from the player's own console is recommended.

## Pinned upstream sources

- PCSX-ReARMed PS3 fork: [Calyps0man/pcsx_rearmed_for_ps3](https://github.com/Calyps0man/pcsx_rearmed_for_ps3), commit `cce9fc67f46a495d4746aa49cff8e3367a9a315e`.
- RetroArch: [libretro/RetroArch](https://github.com/libretro/RetroArch), commit `8ede0d8aea9308e72d4f2b58c3074934803a6452`.

The build applies small, documented compatibility changes for the PSL1GHT static-core link: disabling the physical-disc CD-ROM path when loading BIN/CUE through libretro, reusing RetroArch's thread implementation to avoid duplicate symbols, and applying the Cell PPU/RSX compatibility fixes required by the pinned source.

A successful PKG build confirms compilation and packaging only. It does not confirm a playable session on physical PS3 hardware. Two-console online testing remains an outstanding validation step.

The earlier Tiny3D shell, Vercel guest-room service and custom UDP input transport are separate prototypes; they are not the gameplay transport used by this RetroArch package.
