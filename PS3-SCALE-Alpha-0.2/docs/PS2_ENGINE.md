# PS2 support on PS3 Slim

The CECH-2504A uses software PS2 emulation (`ps2_netemu`) for PS2 Classics rather than the
integrated PS2 Emotion Engine/Graphics Synthesizer found in early FAT models.

The first safe PS2 milestone is:
- Detect PS2 Classics installed under `/dev_hdd0/game/<TITLE_ID>/` using CATEGORY `2P`.
- Detect PS2 ISO files under `/dev_hdd0/PS2ISO/` and mounted USB `PS2ISO` folders.
- Index title ID and matching `ps2_netemu` CONFIG metadata where available.
- Keep original ISO and installed package files read-only.
- Install only verified, per-title CONFIG data through a separate reversible operation.

CONFIG files are emulator configuration/patch data, not a universal neural upscaler. Their
commands are title-specific and can fix compatibility, timing, or rendering behavior. They
must not be generated speculatively or applied to an unverified game ID.


## Verified CONFIG facts for the software-emulated Slim

The CECH-2504A belongs to the software-emulation family that uses `ps2_netemu.self`. PS3 Developer Wiki documents that external per-title CONFIG data is supported by `ps2_netemu`, with a maximum size of 16,384 bytes. CONFIG commands can adjust emulator behavior or apply per-game fixes; they are not a general-purpose AI upscaler.

For PS2 ISO files, the PS3 Classics CONFIG guide describes a sidecar file named exactly after the ISO plus `.CONFIG`, for example `Game.iso.CONFIG`, stored beside the ISO. The ISO itself should remain unchanged. Commands such as 0x21 and 0x46 are community-documented performance experiments, not guaranteed improvements; they must be tested against the exact game and region before being offered.

References:
- [PS3 Developer Wiki — PS2 Emulation / Game CONFIG](https://www.psdevwiki.com/ps3/PS2_Emulation)
- [PS3 Classics — CONFIG Guide](https://github.com/ps3classics/ps2/blob/main/config-guide.html)
- [PS2 Classics compatibility resource](https://ps3classics.github.io/ps2/)
