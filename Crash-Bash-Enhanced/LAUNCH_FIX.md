# PS3 XMB launch fix

## Symptom

The installed XMB entry appeared normally, but selecting it returned to the XMB instead of opening RetroArch.

## Change

The earlier package used a two-stage launch:

1. XMB started a small Salamander EBOOT.
2. Salamander scanned `USRDIR/cores` for a second SELF and invoked a process-exit-spawn operation.

That path depended on core discovery, the user profile path, and a second SELF spawn. The revised package removes that second-stage launch. The XMB now starts a direct NPDRM `EBOOT.BIN` built from the actual RetroArch executable with PCSX-ReARMed statically linked.

The direct EBOOT is generated from PSL1GHT's stripped and SPRX-linked ELF output (the same processed ELF used to create the normal PS3 SELF). It is not wrapped from the raw, unprocessed linker ELF.

The PS3 frontend also sets its active static-core path to the installed app's own `EBOOT.BIN`, and the per-user RetroArch profile's parent directory is created before the app-specific directory.

## XMB artwork

The package now decodes and validates the custom 320x176 Crash Bash Online `ICON0.PNG` cover art before building the PKG.

## Build-time checks

CI checks:

- PS3 toolchain and symbol-inspection utility are available.
- The direct EBOOT has a PS3 SELF header.
- The static libretro entry points are linked into the unstripped RetroArch ELF.
- The SFO contains the expected application ID and title.
- The XMB icon is a valid 320x176 PNG.
- The PKG is finalized (revision `0x8000`) and its SHA-256 checksum matches.

## Hardware validation still required

A successful CI build cannot prove that a physical PS3 boots the app. The first hardware check is simply to launch the app from XMB and confirm that the RetroArch menu opens. Then load the user's own Crash Bash BIN/CUE dump and test local gameplay before attempting Netplay. Do not treat four-player or internet play as validated until tested on actual consoles.
