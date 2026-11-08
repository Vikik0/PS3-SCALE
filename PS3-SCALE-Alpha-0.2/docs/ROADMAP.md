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
- [x] Search through a PS3 SCALE custom controller keyboard (English, Spanish and Arabic layouts); native OSK is not used.
- [x] Scan layout with independent list, metadata and cover columns.
- [x] SELECT and START search shortcuts; UTF-8 query editing and code-point-aware backspace.
- [x] Slow low-poly Tiny3D 3D home emblem with Settings toggle.
- [x] Main menu vector icons and library overview.
- [x] Persistent music, volume, animation and language settings.
- [x] First-run English / Arabic / Spanish language chooser.
- [x] Embedded compact Arabic bitmap glyph atlas with contextual letter forms and RTL run ordering.
- [x] Staged startup screen with local app check and worker-thread library scan.
- [x] Startup MP3 streaming, looping, mute and volume.
- [x] USB-to-HDD music copy with immediate audio-backend restart.
- [x] Offline USB package discovery, library rescan and restore defaults.
- [x] Companion Tools status detection for Apollo, Artemis and webMAN.
- [x] Static UI contract tests included in host CI.
- [ ] Physical smoke test of the localized UI, custom keyboard, 3D animation, startup scan and music handoff.

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


## Localization and startup UX — source implementation
- [x] Add first-run English / Arabic / Spanish selection and persist the choice.
- [x] Add Spanish translations and an embedded Arabic bitmap glyph atlas derived from Noto Sans Arabic.
- [x] Add contextual Arabic letter-form selection and right-to-left display ordering for the translated UI strings.
- [x] Replace the native PS3 OSK with an in-app controller keyboard, including an Arabic key layout and UTF-8-aware backspace.
- [x] Add a staged loading screen and perform local app/tool checks plus library scanning on a worker thread.
- [x] Replace the flat ambient rings with a slow low-poly 3D wireframe emblem and orbit rings.
- [x] Add host tests for locale translations, glyph conversion and keyboard layouts.
- [ ] Verify Arabic glyph appearance, keyboard navigation, frame pacing, startup scan and settings persistence on the target PS3.
- [ ] Complete signed online update-manifest support before any network download is considered.
