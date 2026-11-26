# PS2 game enhancement on PS3 Slim

## Hardware and emulator path

A CECH-25xx PS3 Slim does not contain the PS2 Emotion Engine / Graphics Synthesizer hardware found in early backward-compatible FAT models. PS2 titles run through Sony's software-emulation path, `ps2_netemu`. CFW enables homebrew access; it does not turn a Slim into a hardware-backward-compatible PS3.

PS3 SCALE targets the per-title `ps2_netemu` CONFIG sidecar for PS2 ISO files and the matching encrypted `.ENC` CONFIG route for PS2 Classics. The app enables netemu-only local profiles and code patches only when syscall hardware detection positively identifies a PS3 Slim or Super Slim. FAT hardware (which may use `ps2_emu` / `ps2_gxemu`) and unknown hardware fail closed for these netemu-only commands; the app does not assume that command `0x0A` is portable across emulators. The original ISO, EBOOT and installed game package remain untouched.

## What the PS2 enhancement controls actually do

The local profile builder emits only a small, allow-listed CONFIG:

| Option | CONFIG command | Actual effect | Limit |
|---|---|---|---|
| Sharper pixels | `0x44` | Disables the emulator smoothing filter | Can make edges look more jagged; it is not an HD upscale |
| L2H performance | `0x46` | Enables the documented L2H improvement | Title-dependent; not a universal FPS increase |
| Conservative frame pacing | `0x48` + mode `1` + delay `20000` | Uses the documented No-IPU / conservative VSYNC-delay preset | Timing experiment; not an FPS unlock and not enabled by default |
| Pressure-sensitive controls | `0x50` | Enables analog pressure-sensitive buttons | Optional; off by default; requires CONFIG revision 4.78+ |

The `0x48` values are serialized little-endian as two 32-bit parameters. The command is documented for newer netemu builds (4.45-era and later); the builder deliberately exposes only the conservative preset documented in official CONFIG examples. More aggressive values and the separate `0x21` command are not generated automatically. Generated profiles end with the empty `0x00` terminator. A Title ID / multidisc command must remain the final command and is therefore not modified by the patch appender.

### Display scaling is a separate layer

Sony's XMB exposes `Settings > Game Settings > PS / PS2 Upscaler` and `PS / PS2 Smoothing`. PS3 SCALE now provides a `TRIANGLE: DISPLAY GUIDE` overlay for these global settings.

The official [PS3 Game Settings manual](https://manuals.playstation.net/document/en/ps3/current/settings/gamesettings.html) describes:
- `Normal`: upscale while preserving the original aspect ratio.
- `Full Screen`: stretch to fill the screen and change the aspect ratio.
- `PS / PS2 Smoothing`: reduce jagged edges, with possible softening/blur.

The separate `Display Settings > Video Output Settings` menu chooses supported output modes such as 720p/1080p. This upscales the output signal; it does not render PS2 games natively at HD. These settings are global, and PS3 SCALE does not change them automatically.

## What can and cannot produce 60 FPS

There is no safe universal switch that turns every PS2 game into native 60 FPS. A PS2 game may be designed for 30 FPS, 50 Hz PAL timing, 59.94 Hz NTSC timing, or a variable frame rate. Raising the display output to 60 Hz is not the same as raising the game's simulation and rendering rate.

The PS3 netemu CONFIG format does include command `0x0A` (`EE_INSN_REPLACE32`), which can replace exact 32-bit EE instructions and is documented as netemu-only with a maximum list count of 32. The app therefore fails closed unless a Slim / Super Slim platform ID is positively detected; FAT / hardware-backward-compatible and unknown models cannot apply these records through this path. Each entry contains an EE offset, expected original 32-bit word and replacement word. The builder supports only aligned mode-0 EE addresses below `0x10000000` and rejects duplicates or unverified address modes. This is a plausible foundation for carefully curated per-game code patches. It is not a generic FPS unlock: a valid patch needs the exact game serial, region, revision, EE address/mode, original opcode and replacement opcode. A wrong address or expected opcode can break the title.

PCSX2 `.pnach` files are not drop-in PS3 CONFIG files. Some entries are runtime data writes, some target code, and some depend on emulator-specific behavior. PS3 SCALE now has a deliberately narrow converter for a small, source-pinned set of proof-of-concept patch records. It resolves root-level or nested ISO9660 boot paths, assembles bounded multi-extent files, extracts the boot ELF, matches the PCSX2 serial + XOR ELF hash, maps each EE address through an executable `PT_LOAD` segment, reads the original 32-bit word, and creates a guarded netemu `0x0A` entry. It does not import arbitrary `.pnach` files, accept unsupported patch types, or guess offsets.

Artemis PS3 is not treated as a generic runtime cheat injector for a PS2 game inside `ps2_netemu`. The current catalogue contains eleven experimental 60 FPS candidates. The user must select the Enhancer action to apply a patch; games are not silently modified during launch. The catalogue does not promise a working 60 FPS result until tested on the actual PS3 Slim, and it does not convert arbitrary PCSX2 cheats.

## PS3 SCALE implementation

The PS2 Enhancer screen now provides:
- Probe the selected ISO boot ELF on entry and show exact-match availability for FPS, 16:9 and 480p before the user applies a patch; these actions are gated behind confirmed Slim / Super Slim netemu hardware.
- Validate every candidate patch address against a file-backed executable ELF segment before showing it as available; the apply path repeats this check.

- `0x46` L2H performance toggle (on by default).
- `0x44` smoothing-disable / sharper-pixel toggle (off by default).
- Optional conservative `0x48` frame-pacing toggle (off by default and explicitly experimental).
- Optional `0x50` pressure-sensitive controls toggle (off by default).
- Exact-match 480p progressive-output patches for three PAL titles; unsupported patch types are rejected.
- Local netemu CONFIG generation for PS2 ISO sidecars only on confirmed Slim / Super Slim hardware; PS2 Classics require an exact matching encrypted ENC file.
- Refuse to replace any existing CONFIG with a minimal local profile, preserving community and previously installed settings. The separate game-patch path can append to compatible CONFIGs without discarding their commands.
- Append or safely merge exact-match experimental `0x0A` code-patch entries for FPS, widescreen and 480p. Existing patch lists are merged only when the complete CONFIG uses the validated allow-listed command subset; duplicate addresses, unknown command streams and Title ID / multidisc footers are refused.
- Automatic byte-for-byte backup before replacing an existing CONFIG. If the saved backup no longer matches the current file, restore it before applying a different profile; the backup is never silently replaced.
- Exclusive temporary-file staging, validated generated bytes, atomic install, retained rollback, and restore.
- Download of an exact title/serial CONFIG from the existing catalogue.
- Clear notice that the L2H, smoothing and frame-pacing toggles do not unlock 60 FPS; the separate game-specific patch is experimental. XMB scaling remains separate.

For ISO files the target remains `<game.iso>.CONFIG` beside the ISO. No bytes are written into the ISO itself. PS2 Classics ENC files continue to use the existing webMAN PS2CONFIG directory and exact catalogue matching; the local plaintext builder is intentionally disabled for Classics.

## Initial game-specific patch catalogue

The first FPS candidate is **Ed, Edd n Eddy: The Mis-Edventures (NTSC-U)** from the pinned PCSX2 patch repository:
- Serial: `SLUS_212.60`
- PCSX2 ELF hash: `B0AE2D8A`
- Patch: `patch=1,EE,00161860,word,2C620000`
- The source patch is pinned to [PCSX2 patch repository commit 1fd9810](https://github.com/PCSX2/pcsx2_patches/blob/1fd98102b8e2fffe3e0c29aab55339d48af0ffe5/patches/SLUS-21260_B0AE2D8A.pnach).

The pinned PCSX2 source files also contain exact 16:9 widescreen sections for twelve titles. SCALE converts only their `patch=1,EE,...,word,...` entries into guarded netemu `0x0A` lists, reading every original word from the exact matched boot ELF:

| PS2 title | Serial | ELF hash | 16:9 writes |
|---|---|---:|---:|
| Scooby-Doo! Night of 100 Frights | SLUS_203.49 | 10B1F370 | 8 |
| Teen Titans | SLUS_211.83 | E1F17139 | 13 |
| Disney's Kim Possible - What's the Switch | SLUS_214.37 | 86B490D1 | 7 |
| Jimmy Neutron: Jet Fusion | SLUS_206.96 | FCC09959 | 3 |
| 7 Sins (PAL-M) | SLES_532.80 | 52DEB87B | 9 |
| Shadow Hearts (NTSC-U) | SLUS_203.47 | 8BE3D7B2 | 5 |
| Teenage Mutant Ninja Turtles 2: Battle Nexus (PAL-M5) | SLES_530.17 | C7375C85 | 2 |
| Obscure (NTSC-U) | SLUS_207.77 | A06047D6 | 6 |
| Armored Core 3 (PAL-E) | SLES_513.99 | DE5AF9A8 | 4 |
| Bionicle (NTSC-U) | SLUS_208.18 | 8371B27B | 2 |
| Tom Clancy's Splinter Cell: Double Agent (PAL-M3) | SLES_538.27 | 6BD0E9C2 | 1 |
| La Pucelle: Tactics (PAL) | SLES_529.78 | 474EC389 | 7 |

These are aspect-ratio/game-camera patches, not HD texture replacements or native-resolution upscaling. They remain experimental until tested on the target PS3; the ISO is never rewritten.

### 480p progressive-output patches

The pinned PCSX2 commit also contains simple word-only 480p sections for thirteen PAL titles. SCALE adds these as a separate exact-match patch type:

| PS2 title | Serial | ELF hash | EE word writes |
|---|---|---:|---:|
| Crash of the Titans (PAL-R) | SLES_548.43 | AEFD7D25 | 3 |
| George of the Jungle (PAL-M) | SLES_549.75 | 278B7BEA | 3 |
| Syberia (PAL) | SLES_513.93 | D1ACD489 | 1 |
| Crash of the Titans (PAL-M) | SLES_548.41 | ECA6BFC5 | 3 |
| Medal of Honor: European Assault (PAL-M) | SLES_533.32 | BB70FFB9 | 3 |
| SpongeBob SquarePants: Battle for Bikini Bottom (PAL-F) | SLES_536.23 | 9ADA98B4 | 5 |
| Star Wars: Episode III - Revenge of the Sith (PAL-M3) | SLES_531.55 | 99AD19EE | 3 |
| Commandos: Strike Force (PAL-M) | SLES_527.68 | 5E1161DF | 4 |
| The Punisher (PAL-S-I) | SLES_530.49 | 5AC5D875 | 3 |
| Driver: Parallel Lines (PAL-M) | SLES_540.27 | D720770D | 3 |
| kill.switch (PAL-M) | SCES_521.24 | 91A65EAE | 3 |
| Space Chimps (PAL-M) | SLES_553.40 | C423A2DC | 3 |
| Cold Fear (PAL-M) | SLES_531.58 | E38A0AB6 | 2 |

Only `word` writes are converted; files using `extended`, byte writes or conditional runtime codes are excluded. The kill.switch source notes that FMVs remain PAL even when the 480p startup mode is selected. This selects a 480p game mode where the title supports it; the PS3 XMB upscaler can then scale the output to the selected display mode. It is not a native 720p/1080p renderer, and some titles may show offset or FMV issues, so test per title and restore the backup if needed.

PCSX2's patch filename hash is not standard CRC32: its game ELF hash is the XOR of little-endian 32-bit words across the boot ELF. PS3 SCALE resolves the BOOT/BOOT2 path from SYSTEM.CNF through nested ISO9660 directories, computes this same hash, and reads the original word only from a file-backed executable MIPS ELF segment. The converter can then create a netemu `0x0A` replacement entry with the exact original word as a guard.

The current experimental catalogue contains eleven exact serial + ELF-hash candidates from the same pinned PCSX2 patch commit:

| PS2 title | Serial | PCSX2 ELF hash |
|---|---|---|
| Ed, Edd n Eddy: The Mis-Edventures | SLUS_212.60 | B0AE2D8A |
| Scooby-Doo! Night of 100 Frights | SLUS_203.49 | 10B1F370 |
| Teen Titans | SLUS_211.83 | E1F17139 |
| Disney's Kim Possible - What's the Switch | SLUS_214.37 | 86B490D1 |
| Happy Feet | SLUS_214.55 | BAFEFC57 |
| The Ant Bully | SLUS_214.15 | E10A865B |
| Indiana Jones and the Staff of Kings | SLUS_218.85 | B15F0BD6 |
| Jimmy Neutron: Jet Fusion | SLUS_206.96 | FCC09959 |
| Shin Megami Tensei - Devil Summoner 1 | SLPM_662.46 | 67D76160 |
| Manhunt | SLUS_208.27 | 38DEA143 |
| Pirates of the Caribbean: The Legend of Jack Sparrow | SLUS_211.10 | 632E3976 |

The Manhunt and Pirates of the Caribbean candidates each use one direct aligned `word` replacement from their pinned PCSX2 `[60 FPS]` sections; neither section notes an EE-overclock requirement. For Manhunt, only the FPS section is imported; its separate PCSX2 crane-bug fix is not included.

The Devil Summoner candidate uses the pinned PCSX2 `[60 FPS]` section's single aligned word replacement at `0x0011B560`; its same file's widescreen and no-interlacing sections are not imported. No EE-overclock requirement is noted in that FPS section.

Each source file's `[60 FPS]` section contains a single `patch=1,EE,...,word,...` replacement. The catalogue deliberately excludes PCSX2 extended writes, mode-2 addresses and patches whose notes require EE overclocking. These remain candidates, not hardware-verified PS3 patches. Every candidate must be tested on the target PS3 Slim for actual frame rate, timing, audio, cutscenes and stability before being marked verified.

Root-level and nested ISO9660 multi-extent SYSTEM.CNF / BOOT files are assembled from bounded, ordered extent chains. The reader rejects mismatched identifiers, unsupported special-file flags, extended attributes, interleaved files, non-sector-aligned continuation extents, oversized chains and chains longer than 32 records. Host fixtures exercise successful split reads and malformed-chain rejection.

Physical-console acceptance criteria and the per-title result register are maintained in [PS2_HARDWARE_TEST_REPORT.md](PS2_HARDWARE_TEST_REPORT.md).

## Next stage: broaden PS2 game enhancement coverage

1. Build a per-title validation matrix for 60 FPS, 16:9, 480p/Y-fix, FMV playback and controller behavior.
2. Add only source-pinned, exact serial + ELF-hash FPS, widescreen and 480p patches that map safely to netemu `0x0A`; reject runtime writes, extended codes and emulator-specific behavior.
3. Test each candidate on the actual PS3 Slim; record frame rate, output mode, gameplay timing, audio, cutscenes and stability.
4. Expand the catalogue only after physical-console testing, and keep unsupported revisions disabled.
5. Never advertise 60 FPS unless the game itself sustains the target rate without broken physics, audio, cutscenes or timing.

## References

- [PS3 Developer Wiki — PS2 Emulation](https://www.psdevwiki.com/ps3/PS2_Emulation)
- [PS3 Developer Wiki — PS2 CONFIG command reference](https://www.psdevwiki.com/ps3/PS2_Emulation/PS2_Config_Commands)
- [webMAN MOD — ps2_netemu CONFIG command details](https://github.com/aldostools/webMAN-MOD/blob/6bd4425229544e60c5c8cfdcadb664dfb163d9f3/_Projects_/tools/ps2_netemu_config_commands.html)
- [PS3 Developer Wiki — official PS2 CONFIG examples](https://www.psdevwiki.com/ps3/PS2_Official_Configs)
- [PS3 Classics — CONFIG guide](https://github.com/ps3classics/ps2/blob/main/config-guide.html)
- [PS3 Classics — compatibility resources](https://ps3classics.github.io/ps2/)
- [Sony PS3 manual — Game Settings](https://manuals.playstation.net/document/en/ps3/3_10/settings/gamesettings.html)
- [PCSX2 — writing patches](https://pcsx2.net/docs/advanced/writing-patches)
- [PCSX2 patch repository](https://github.com/PCSX2/pcsx2_patches)
- [PSX-Place — PS2 netemu CONFIG discussion](https://psx-place.com/threads/w-i-p-configs-for-ps2_netemu-explained.15034/)

Host-side tests validate CONFIG serialization and safe file staging. They do not prove game compatibility or a real FPS gain. Physical-console validation remains required.
