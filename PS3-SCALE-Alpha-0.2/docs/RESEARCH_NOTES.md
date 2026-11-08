# PS3 SCALE — Developer research notes (8 Nov 2026)

This document records design decisions from public developer documentation and source repositories.
It is a research map, not an endorsement to copy third-party code into PS3 SCALE.

## 1. Development toolchain and rendering

- PSL1GHT is the open-source GameOS homebrew SDK. Its current project documentation points developers to the `ps3dev` master environment, which combines the toolchain, libraries and SDK.
- The `02900/ps3-toolchain` project publishes separate container images for the core toolchain and renderer stacks. Its Tiny3D image includes PSL1GHT, Tiny3D and YA2D dependencies, making it a better CI starting point than the legacy `hldtux/ps3dev` container we first tried.
- Tiny3D is a small RSX 2D/3D renderer. It is appropriate for our app UI, icons, textures and diagnostic visualizations. It is not, by itself, a system-wide post-processing layer for other games.

## 2. What to learn from multiMAN, webMAN MOD and IRISMAN

- webMAN MOD is an actively maintained CFW/HEN plugin with web/FTP services, PS3MAPI, game mounting, NTFS/exFAT support and system utilities. Its public README documents Cobra CFW support through 4.93. On our CEX 4.93 / Cobra 8.5 test target, it is a useful optional integration point, not a dependency for the basic scanner.
- IRISMAN demonstrates useful user-experience patterns: category/device/name filtering, coverflow and grid layouts, ICON0/PIC0 assets, file-manager navigation and background music. We can reimplement the UX independently.
- IRISMAN is GPL-3.0 and its README asks modified forks to use a different name. PS3 SCALE is a separate project; do not copy its source files into our tree without a deliberate license review and required compliance.
- multiMAN/webMAN already solve game mounting and backup management. PS3 SCALE should focus on game metadata, verified enhancement profiles, reversible configuration management and graphics research rather than becoming a second backup manager.

## 3. Game discovery and PARAM.SFO

Relevant categories documented by PS3 Developer Wiki:
- `DG`: PS3 disc game.
- `HG`: installed HDD game/application.
- `2P`: PS2 Classics package.
- `2G`: PS2 disc installation on backward-compatible models.
- `GD`: PS3 game data, updates and DLC — do not list as a playable game by default.

Implementation decisions:
- Parse `PARAM.SFO` as read-only and use `TITLE`, `TITLE_ID`, `CATEGORY`, and `APP_VER`.
- A mounted PS3 disc is at `/dev_bdvd/PS3_GAME/PARAM.SFO`; it must be scanned directly, not as a child of `/dev_bdvd/GAMES`.
- Keep multiple copies of the same Title ID if they live on different paths/devices.
- PS3 ISO metadata may not be readable from the filename alone. If a Title ID is present in a conventional filename, record it as a hint; otherwise leave it unresolved until the image is mounted or a reliable disc parser is implemented.

## 4. PS2 on CECH-2504A

The PS3 Developer Wiki documents three PS2 emulator families:
- CECHA/B: hardware-assisted `ps2_emu`.
- CECHC/E: `ps2_gxemu` with the PS2 Graphics Synthesizer hardware.
- Later models, including the Slim test target, use software `ps2_netemu`.

For this Slim:
- The first reliable enhancement route is per-title `ps2_netemu` CONFIG files, not a universal shader.
- The public PS2 CONFIG guide documents small binary configs and examples that can address FMV stutter, pressure sensitivity and some performance issues. It explicitly warns that configs are not required for every title and are not guaranteed to fix every issue.
- For PS2 ISO images, the guide's workflow places `Game.iso.CONFIG` beside `Game.iso`. We should preserve the ISO and install/copy a verified config only after matching the serial and offering rollback.
- The PS2 Netemu CONFIG maximum documented by PS3 Developer Wiki is 16,384 bytes.
- The new ISO probe reads the ISO9660 PVD and root `SYSTEM.CNF` to extract serial IDs such as `SLUS_203.12`. It is a metadata probe only; it never writes to the image.

## 5. PS3 performance and graphics patches

- Public community work now provides title-specific FPS unlock patches for real PS3 hardware, including a smaller set explicitly marked hardware-confirmed and a much larger mixed-confidence list converted from RPCS3 patches.
- These are not universal resolution upscalers and not frame generation. They are title/version-specific memory or executable patches.
- We must require exact Title ID and game version, display hardware-test confidence, hash every downloaded patch, back up files before permanent changes, and offer rollback.
- Do not bulk-enable the unverified patch list. A repository currently warns about known crashes for some patches, including a GTA V patch; the catalog must carry denylist/known-issue metadata.

## 6. Realistic graphics-engine architecture

A standalone manager application exits when a user launches a separate game from XMB. Therefore its Tiny3D renderer cannot keep post-processing that game's framebuffer.

Candidate enhancement routes, in order:
1. Safe system-level display settings the user can enable (PS2 smoothing/upscaler, output mode) — these affect output scaling, not necessarily internal game render resolution.
2. Verified title-specific game patches (resolution, frame pacing, FPS caps, widescreen, graphics flags).
3. A CFW-only per-title PRX/plugin hook, researched separately and enabled only after game-specific hardware tests.
4. External HDMI scaler as a universal last-stage solution if the user wants post-processing independent of game code.

Frame generation is not enabled in Alpha 0.2. A real implementation would need access to consecutive game frames and GPU/memory bandwidth while preserving latency and frame pacing. No generic "30 to 60 FPS" claim will be shown without a measured, title-specific implementation.

## 7. Online catalog safety model

Every enhancement record must contain:
- platform, Title ID or PS2 serial, region and exact game update version;
- patch type and installation target;
- SHA-256, byte size and source URL;
- author, license, hardware tested and confidence level;
- known issues, backup procedure and rollback procedure.

Downloads are data only. The app must never execute downloaded code or trust an arbitrary URL returned by a server.


## 8. Primary references

- PSL1GHT SDK: https://github.com/ps3dev/PSL1GHT
- PS3 toolchain: https://github.com/ps3dev/ps3toolchain
- Tiny3D toolchain container (used by this project's CI): https://github.com/02900/ps3-toolchain
- Tiny3D renderer: https://github.com/wargio/tiny3D
- webMAN MOD source and compatibility notes: https://github.com/aldostools/webMAN-MOD
- IRISMAN source and feature list: https://github.com/aldostools/IRISMAN
- PS3 Developer Wiki — PARAM.SFO: https://www.psdevwiki.com/ps3/PARAM.SFO
- PS3 Developer Wiki — PS2 Emulation: https://www.psdevwiki.com/ps3/PS2_Emulation
- PS2 Classics CONFIG guide: https://github.com/ps3classics/ps2/blob/main/config-guide.html
- PS2 Classics community compatibility resource: https://ps3classics.github.io/ps2/
- Community PS3 FPS patch catalogue: https://github.com/DoSpamu/RPCS3toArtemisPatches
