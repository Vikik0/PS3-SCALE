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
