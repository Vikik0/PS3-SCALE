# Crash Bash PS3 — PS1 execution base

The current online package uses the **RetroArch PSL1GHT frontend with PCSX-ReARMed statically linked as its PS1 core**. The PS3 XMB package starts RetroArch; it does not contain a native reimplementation of Crash Bash's original engine.

Online play in this package is provided by **RetroArch Netplay**. PCSX-ReARMed supplies PS1 emulation, serialization and Multitap support; RetroArch handles host/client synchronization, the public host list, NAT traversal and relay connections. The build configures Multitap on Port 1 and allows one host plus up to three remote clients. Four-participant performance remains unverified on physical PS3 hardware.

## Netplay synchronization profile

The pinned PCSX-ReARMed source reports a serialization buffer of `0x440000` bytes (4.25 MiB). The package therefore enables RetroArch's **Lockstep** mode. Lockstep keeps one shared savestate rather than a rollback state for every buffered frame. It reduces memory use, but every frame waits for all participating players' input, so network latency can appear as added input delay or stalls.

RetroArch's [official PS1 guide](https://retroarch.com/ps1.html) identifies PCSX-ReARMed as the PS1 core with Netplay and Multitap support. Its [Netplay FAQ](https://docs.libretro.com/guides/netplay-faq/) warns that PSX netplay can be too demanding. Lockstep is a mitigation, not proof that the PS3 has enough CPU headroom.

## Content and BIOS requirements

The package does not redistribute Crash Bash disc data or a PlayStation BIOS. Each player must use the same exact CUE file and byte-identical referenced BIN track files, as well as the same region, BIOS file (or the same HLE mode), core version and core options.

A BIOS dumped from the user's own PS1 is recommended. The system directory is:

`/dev_hdd0/game/CBON00001/USRDIR/system/`

RetroArch settings, saves and logs use an isolated app profile:

`/dev_hdd0/home/<user-id>/retroarch/CBON00001/`

## Pinned upstream sources

- PCSX-ReARMed PS3 fork: [Calyps0man/pcsx_rearmed_for_ps3](https://github.com/Calyps0man/pcsx_rearmed_for_ps3), commit `cce9fc67f46a495d4746aa49cff8e3367a9a315e`.
- RetroArch: [libretro/RetroArch](https://github.com/libretro/RetroArch), commit `8ede0d8aea9308e72d4f2b58c3074934803a6452`.

The build applies small, documented compatibility changes for the PSL1GHT static-core link: disabling the physical-disc CD-ROM path when loading BIN/CUE through libretro, reusing RetroArch's thread implementation to avoid duplicate symbols, applying the Cell PPU/RSX compatibility fixes, setting the Multitap default and isolating the PS3 user profile.

A successful PKG build confirms compilation and packaging only. It does not confirm full-speed gameplay or a stable online session on physical PS3 hardware. Two-console LAN and internet testing, followed by four-player testing, remain outstanding validation steps.
