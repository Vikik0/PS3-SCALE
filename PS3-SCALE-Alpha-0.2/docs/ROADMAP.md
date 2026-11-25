# PS3 SCALE roadmap

## Alpha 0.33 — support reporting reliability

- [x] Extend report title/details to the backend's 100 / 2,000 character limits.
- [x] Add bounded JSON serialization with escaping and UUIDv4 request IDs.
- [x] Persist each report atomically as a separate JSON outbox file.
- [x] Add pending-report recovery and a Help & Support retry action after app restart.
- [x] Make support retries idempotent and protect the endpoint with atomic HMAC-based daily rate limiting.
- [x] Pin the Edge Function SDK and commit the Deno dependency lock.
- [x] Add Deno type-check, host payload tests and support security contract tests.
- [ ] Create the isolated Supabase project in the user-confirmed organization, apply migration and deploy Edge Function.
- [ ] Configure the project URL and publishable key, then build and test the live reporting path.
- [ ] Validate offline/online retry behavior on physical PS3 hardware.

## Alpha 0.32 — persistence and download safety

- [x] Atomic settings and favorites writes using unique exclusive temp files, fsync and same-directory rename.
- [x] Protect one-launch webMAN script creation from existing temp files and symlink targets.
- [x] Use unique O_EXCL HTTPS download temp paths so stale files do not block future refreshes.
- [x] Preserve pre-existing catalog, community NCL and PS2 CONFIG destinations when a download fails.
- [x] Add host regression tests for symlink handling, atomic persistence and download failure cleanup.
- [ ] Validate settings, favorites, network refresh and PS2 CONFIG behavior on physical PS3 hardware.
- [ ] Deploy the optional support-report endpoint only to a dedicated PS3 SCALE Supabase project with server-side secrets.


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
- [x] Theme MP3 embedded in the PKG; no manual USB copy action required.
- [x] Offline USB package discovery, library rescan and restore defaults.
- [x] Companion Tools status detection for Apollo, Artemis and webMAN.
- [x] Static UI contract tests included in host CI.
- [x] Game Title ID + APP_VER revalidation runs in background workers with static scan storage, not on the controller stack.
- [x] Safe unmodified game-launch fallback when profile arming fails for recoverable reasons; preserve selection checks and foreign webMAN scripts.
- [x] Bound cover/preview file sizes before PNG decoding.
- [ ] Physical smoke test of the localized UI, custom keyboard, 3D animation, startup scan, game launch and music handoff.

## Alpha 0.3 — PS2 configuration manager

- [x] Restricted in-memory PS2 netemu CONFIG builder for commands 0x44/0x46, with host tests.
- [x] Triangle-triggered PS2 ISO CONFIG preview (preview only; no writes).
- [x] Scan and launch webMAN-supported PS2 ISO `.ISO` / `.BIN` / `.IMG` / `.MDF` formats; keep `.BIN.ENC` classified as PS2 Classics.
- [x] PS2 Classics/PS2 ISO metadata enrichment: retain Title ID / disc serial and infer a conservative region label; unknown IDs remain UNKNOWN.
- [x] Match known ps2_netemu CONFIG files by exact normalized Title ID or disc serial; IDs are region-specific and no cross-region fallback is used.
- [x] Byte-for-byte verified backup, atomic restore and guarded replacement with retained rollback copy (host-tested).
- [x] Reject symlinked CONFIG/backup/temp/rollback paths in host tests; use exclusive temp creation and atomic replacement on PS3 without writing through symlink destinations or deleting the current file first.
- [x] No automatic CONFIG writes; all file changes require an explicit action in PS2 CONFIG Manager.
- [ ] Physical validation of backup, restore and one matched CONFIG on the target PS3 Slim.

## Alpha 0.4 — PS3 title-specific patch manager

- [ ] Cryptographically sign the online catalog and downloads; Git blob SHA-1 verification is an integrity check, not a signature.
- [x] Exact Title ID + inferred region + APP_VER matching for FPS profiles and community NCL installs; mismatched region or update version is rejected.
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
- [x] Verify community NCL downloads against the indexed GitHub Git-blob SHA-1 and exact byte size before installation.
