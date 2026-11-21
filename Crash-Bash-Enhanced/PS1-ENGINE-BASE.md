# Crash Bash PS3 base

This is the real PS1-game execution base for the Crash Bash PS3 project: a
PS3-native PCSX-ReARMed front end and core, built from the pinned upstream
PSXONPS3 source. The package does not redistribute Crash Bash disc data. Use
the BIN/CUE from your own disc and place it in \`/dev_hdd0/PSXISO/\` or a USB
\`/PSXISO/\` folder.

The existing \`crashbash.pkg\` scaffold is not the game engine. This build path
replaces that placeholder with a PS1 emulator capable of running the original
game. The current base build is offline; the online netplay layer is a separate
remaining integration and hardware-test stage. Do not treat a successful PKG
build as proof of online play.

Upstream source: [Calyps0man/pcsx_rearmed_for_ps3](https://github.com/Calyps0man/pcsx_rearmed_for_ps3),
pinned at \`cce9fc67f46a495d4746aa49cff8e3367a9a315e\`. It is based on PCSX-ReARMed
and is distributed under GPL-2. The package includes the GPL-2 license text.
