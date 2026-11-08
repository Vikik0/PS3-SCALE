# PS3 SCALE roadmap

## Alpha 0.1 — safe library foundation

- [x] PS3 PARAM.SFO parser.
- [x] Folder game scanner (GAMES, GAMEZ).
- [x] Installed-title scanner (HG, 2P, 2G).
- [x] PS3 ISO / PS2 ISO filename discovery.
- [x] PS2 ISO9660 SYSTEM.CNF serial probe.
- [x] PS3 Title ID hint extraction from common ISO filenames.
- [x] Direct mounted Blu-ray scan path.
- [x] Internal HDD, Blu-ray mount and USB000–USB007 scan paths.
- [x] Read-only scan behavior.
- [ ] Physical validation on CECH-2504A / CFW 4.93.

## Alpha 0.2 — library and profile UX

- [x] PS3 and PS2 library screens, sorting and local cover display.
- [x] Search through native PS3 OSK.
- [x] Scan layout with independent list, metadata and cover columns.
- [x] SELECT and START search shortcuts; OSK initial panel included in allowed panel mask.
- [x] Slow Tiny3D home animation with Settings toggle.
- [x] Main menu vector icons and library overview.
- [x] Persistent music, volume and animation settings.
- [x] Startup MP3 streaming, looping, mute and volume.
- [x] USB-to-HDD music copy with immediate audio-backend restart.
- [x] Offline USB package discovery, library rescan and restore defaults.
- [x] Companion Tools status detection for Apollo, Artemis and webMAN.
- [x] Static UI contract tests included in host CI.
- [ ] Physical smoke test of Run #115 UI and music handoff.

## Alpha 0.3 — PS2 configuration manager

- [x] Restricted in-memory PS2 netemu CONFIG builder for commands 0x44/0x46, with host tests.
- [x] Triangle-triggered PS2 ISO CONFIG preview (preview only; no writes).
- [ ] PS2 Classics/PS2 ISO metadata enrichment.
- [ ] Match known ps2_netemu CONFIG files by title ID and region.
- [ ] Hash verification, backup and restore.
- [ ] No automatic CONFIG writes until hardware tests pass.

## Alpha 0.4 — PS3 title-specific patch manager

- [ ] Signed online catalog/download.
- [ ] Exact Title ID + region + game update matching.
- [ ] Patch preview, backup, apply and rollback.
- [ ] First supported title only after repeatable hardware test.
- [ ] CFW runtime component for game-start verification and notification.

## Later research

- [x] Document optional webMAN MOD / PS3MAPI read-only-first route.
- [ ] CFW PRX/per-title runtime hook feasibility.
- [ ] RSX resource/memory budget.
- [ ] Game-specific framebuffer or shader experiments.
- [ ] Frame interpolation feasibility and performance cost.

A release is not considered console-tested until its PKG is installed, launched and exercised on physical hardware. Host CI does not replace console testing.
