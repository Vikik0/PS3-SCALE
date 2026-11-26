# PS2 game enhancement on PS3 Slim

## Hardware and emulator path

A CECH-25xx PS3 Slim does not contain the PS2 Emotion Engine / Graphics Synthesizer hardware found in early backward-compatible FAT models. PS2 titles run through Sony's software-emulation path, `ps2_netemu`. CFW enables homebrew access; it does not turn a Slim into a hardware-backward-compatible PS3.

PS3 SCALE therefore targets the per-title `ps2_netemu` CONFIG sidecar for PS2 ISO files and the matching encrypted `.ENC` CONFIG route for PS2 Classics. The original ISO, EBOOT and installed game package remain untouched.

## What the PS2 enhancement controls actually do

The local profile builder emits only a small, allow-listed CONFIG:

| Option | CONFIG command | Actual effect | Limit |
|---|---|---|---|
| Sharper pixels | `0x44` | Disables the emulator smoothing filter | Can make edges look more jagged; it is not an HD upscale |
| L2H performance | `0x46` | Enables the documented L2H improvement | Title-dependent; not a universal FPS increase |
| Conservative frame pacing | `0x48` + mode `1` + delay `20000` | Uses the documented No-IPU / conservative VSYNC-delay preset | Timing experiment; not an FPS unlock and not enabled by default |

The `0x48` values are serialized little-endian as two 32-bit parameters. The command is documented for newer netemu builds (4.45-era and later); the builder deliberately exposes only the conservative preset documented in official CONFIG examples. More aggressive values and the separate `0x21` command are not generated automatically. Generated profiles end with the empty `0x00` terminator. A Title ID / multidisc command must remain the final command and is therefore not modified by the patch appender.

### Display scaling is a separate layer

Sony's XMB exposes `Settings > Game Settings > PS / PS2 Upscaler` and `PS / PS2 Smoothing`. Upscaler choices affect how the SD output is scaled to the display; they do not add HD textures or change the game's internal render resolution. Smoothing reduces roughness but can soften the image. These are system-level display preferences; PS3 SCALE does not claim to change them through an undocumented registry write.

Suggested manual check:
1. Open XMB `Settings > Game Settings`.
2. Choose `PS / PS2 Upscaler`: `Normal` preserves the aspect ratio; `Full Screen` stretches to fill the display.
3. Try `PS / PS2 Smoothing` both `On` and `Off`; the best result varies by game and display.

## What can and cannot produce 60 FPS

There is no safe universal switch that turns every PS2 game into native 60 FPS. A PS2 game may be designed for 30 FPS, 50 Hz PAL timing, 59.94 Hz NTSC timing, or a variable frame rate. Raising the display output to 60 Hz is not the same as raising the game's simulation and rendering rate.

The PS3 netemu CONFIG format does include command `0x0A` (`EE_INSN_REPLACE32`), which can replace exact 32-bit EE instructions and is documented as netemu-only with a maximum list count of 32. Each entry contains an EE offset, expected original 32-bit word and replacement word. The builder supports only aligned mode-0 EE addresses below `0x10000000` and rejects duplicates or unverified address modes. This is a plausible foundation for carefully curated per-game code patches. It is not a generic FPS unlock: a valid patch needs the exact game serial, region, revision, EE address/mode, original opcode and replacement opcode. A wrong address or expected opcode can break the title.

PCSX2 `.pnach` files are not drop-in PS3 CONFIG files. Some entries are runtime data writes, some target code, and some depend on emulator-specific behavior. A future converter must reject unsupported patch types, map the exact EE address to the exact game's loaded ELF, verify the original 32-bit word, and generate only supported `0x0A` entries. It must not guess offsets or silently apply a patch from another region/revision.

Artemis PS3 is not treated as a generic runtime cheat injector for a PS2 game inside `ps2_netemu`. The current implementation therefore does not promise 60 FPS and does not convert arbitrary PCSX2 cheats.

## PS3 SCALE implementation

The PS2 Enhancer screen now provides:

- `0x46` L2H performance toggle (on by default).
- `0x44` smoothing-disable / sharper-pixel toggle (off by default).
- Optional conservative `0x48` frame-pacing toggle (off by default and explicitly experimental).
- Local CONFIG generation for PS2 ISO sidecars only; PS2 Classics require an exact matching encrypted ENC file.
- Refuse to replace an existing community CONFIG with a minimal local profile, preserving any compatibility or code-patch commands it contains.
- Append a verified `0x0A` code-patch command to a compatible CONFIG while preserving existing commands; refuse configs with an existing code-patch command or a trailing Title ID / multidisc enforcer until they can be merged safely.
- Automatic byte-for-byte backup before replacing an existing CONFIG. If the saved backup no longer matches the current file, restore it before applying a different profile; the backup is never silently replaced.
- Exclusive temporary-file staging, validated generated bytes, atomic install, retained rollback, and restore.
- Download of an exact title/serial CONFIG from the existing catalogue.
- Clear notice that these settings do not unlock 60 FPS and that XMB scaling is separate.

For ISO files the target remains `<game.iso>.CONFIG` beside the ISO. No bytes are written into the ISO itself. PS2 Classics ENC files continue to use the existing webMAN PS2CONFIG directory and exact catalogue matching; the local plaintext builder is intentionally disabled for Classics.

## First game-specific FPS patch proof of concept

The first candidate is **Ed, Edd n Eddy: The Mis-Edventures (NTSC-U)** from the PCSX2 patch repository (now represented in the exact-match patch catalogue):
- Serial: `SLUS_212.60`
- PCSX2 ELF hash: `B0AE2D8A`
- Patch: `patch=1,EE,00161860,word,2C620000`
- The source patch is pinned to [PCSX2 patch repository commit 1fd9810](https://github.com/PCSX2/pcsx2_patches/blob/1fd98102b8e2fffe3e0c29aab55339d48af0ffe5/patches/SLUS-21260_B0AE2D8A.pnach).

PCSX2's patch filename hash is not standard CRC32: its game ELF hash is the XOR of little-endian 32-bit words across the boot ELF. PS3 SCALE now extracts the root-level BOOT/BOOT2 ELF from SYSTEM.CNF, computes this same hash, and reads the original word only from a file-backed executable MIPS ELF segment. The converter can then create a netemu `0x0A` replacement entry with the exact original word as a guard.

This is a proof-of-concept conversion path, not yet a hardware-verified PS3 patch. The patch must be tested on the target PS3 Slim for actual frame rate, timing, audio, cutscenes and stability before being marked verified.

## Next stage: game-specific FPS patch catalogue

1. Add a PS2 executable reader for ISO9660 and ELF load segments.
2. Define a signed/hashed patch record containing serial, region, revision/hash, EE address, expected original opcode and replacement opcode.
3. Accept only patch operations that can be represented safely by netemu `0x0A`; reject unsupported PCSX2 patch types.
4. Generate a per-game CONFIG and keep the current original CONFIG as a rollback.
5. Test one title at a time on the actual PS3 Slim, record the game's real frame rate and stability, and only then label a patch as verified.
6. Never advertise 60 FPS unless the game itself sustains the target rate without broken physics, audio, cutscenes or timing.

## References

- [PS3 Developer Wiki — PS2 Emulation](https://www.psdevwiki.com/ps3/PS2_Emulation)
- [PS3 Developer Wiki — PS2 CONFIG command reference](https://www.psdevwiki.com/ps3/PS2_Emulation/PS2_Config_Commands)
- [PS3 Developer Wiki — official PS2 CONFIG examples](https://www.psdevwiki.com/ps3/PS2_Official_Configs)
- [PS3 Classics — CONFIG guide](https://github.com/ps3classics/ps2/blob/main/config-guide.html)
- [PS3 Classics — compatibility resources](https://ps3classics.github.io/ps2/)
- [Sony PS3 manual — Game Settings](https://manuals.playstation.net/document/en/ps3/3_10/settings/gamesettings.html)
- [PCSX2 — writing patches](https://pcsx2.net/docs/advanced/writing-patches)
- [PCSX2 patch repository](https://github.com/PCSX2/pcsx2_patches)
- [PSX-Place — PS2 netemu CONFIG discussion](https://psx-place.com/threads/w-i-p-configs-for-ps2_netemu-explained.15034/)

Host-side tests validate CONFIG serialization and safe file staging. They do not prove game compatibility or a real FPS gain. Physical-console validation remains required.
