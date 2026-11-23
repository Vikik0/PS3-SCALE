# Build status — PS3 SCALE Alpha 0.29

## Current development head

- Branch: `design/splash-transitions-ui-v6`
- Pull request: Draft PR #16, targeting `design/settings-support-ui-v5`
- Application version shown in the UI: `ALPHA 0.29`
- Package metadata: `APP_VER=00.29`, `VERSION=00.29`
- Package title ID: `PS3SCALE1`

## Alpha 0.29 — launch freeze and stack-safety hardening

- Moved exact Title ID + APP_VER revalidation for profile arming and profile-backed launches into background workers. HDD and mounted-disc scans no longer run in the controller input loop.
- Replaced the 256-entry local scan lists (about 300 KiB each) with one serialized static validation buffer, avoiding PPU stack exhaustion on the main thread and 0x8000-byte workers.
- If the game identity or update version changes between arming and launch, PS3 SCALE removes only its own pending profile script and refuses the enhanced launch.
- Added bounded file-size checks before decoding local cover and before/after PNGs; missing, non-regular or oversized images are skipped.
- Added a dedicated host regression test for worker-only validation, static scan storage and bounded preview loading.
- GitHub Actions run #573: **PASS**. Host regression tests, PS3 ELF link, CEX SELF generation, standard PKG generation, GNPDRM PKG generation and package integrity checks all passed. Physical-console verification remains required.
- Run: https://github.com/Vikik0/PS3-SCALE/actions/runs/45086065984
- Build artifact: `PS3-SCALE-Alpha-0.29-build-with-embedded-theme` (24,692,403 bytes).
- ELF: 3,504,263 bytes — SHA-256 `1cfa4cd1d468b10e4fb5fdf7b7ca6790b27a09c8e33f6e2d1b404659d26772d8`
- SELF: 779,456 bytes — SHA-256 `087137f815e3502cba7be1fded27177b565b137c447894f57dfb72331ffba129`
- Standard PKG: 11,402,544 bytes — SHA-256 `42eb0d4ae13787d317dde21cd1672409c89c1aa14f628fdec215a455b9ae933b`
- GNPDRM PKG: 11,402,544 bytes — SHA-256 `3915cffd99d6dcf4b4a0fea061df226d206c5cddfaa8e28f9625a68acda72466`
- ZIP SHA-256: `9ba743c22176cb6377cbae1f75c9cc1fb9db0cd4c82a972177827f583d0ade5a`
- Target console: PS3 Slim / CFW 4.93 / Cobra 8.5 (user-reported); actual game launch and controller checks still need to be performed on the console.

## Alpha 0.28 — PS2 ISO format compatibility

- Extended PS2ISO scanning to recognize `.ISO`, `.BIN`, `.IMG` and `.MDF` case-insensitively.
- Extended the webMAN launch-route validator to accept the same four standard PS2 ISO extensions, so a listed IMG/MDF title can be launched through the normal path.
- Kept `.BIN.ENC` classified separately as a PS2 Classic package; it is not treated as a standard ISO.
- Added host scanner fixtures that verify IMG/MDF entries and recover their serials from filenames, plus route tests for encoded IMG/MDF paths.
- Alpha 0.28 GitHub Actions run #562: **PASS**. Host tests, PS3 ELF/SELF build, standard PKG and GNPDRM PKG generation and package checks all passed. Physical-console scan / launch checks remain pending.

## Alpha 0.27 — Companion launch reliability

- Removed the hard block that prevented Apollo / Artemis launch when the webMAN plugin file was not found at the two common paths. Those paths are now treated as hints only; the bounded local HTTP request is the authoritative runtime check.
- The Companion Tools UI now labels the path check as `FILE HINT`, and tells the user when it is trying the local service despite a missing hint.
- Added shutdown joining for the companion launch worker so the app cannot tear down shared state while a local webMAN request is still running.
- Added static regression checks for the fallback path and shutdown join.
- Alpha 0.27 GitHub Actions run #561: **PASS**. Host tests and PS3 ELF/SELF/PKG builds passed. Physical-console Apollo / Artemis launch checks remain pending.

## Alpha 0.26 — Companion Tools launch integration

- Added a controller-selectable Apollo / Artemis launcher to Companion Tools.
- Apollo and both supported Artemis package IDs are routed through the validated webMAN /play.ps3?<TITLE_ID> path.
- The request runs on a worker thread with bounded local HTTP timeouts; failed requests leave PS3 SCALE open.
- Added host tests for valid/invalid app IDs and static UI contract checks for navigation, launch and refresh actions.
- Apollo save operations and Artemis NCL execution remain in their own applications; this change does not claim in-app reimplementation.
- Alpha 0.26 CI run #559: **PASS**. Host tests and PS3 ELF/SELF/standard PKG/GNPDRM PKG builds all passed.
- Physical-console launch checks for Apollo and both Artemis package IDs remain pending.

## Latest code/package CI validation

GitHub Actions run **#562** completed successfully on 2026-11-21.

- Host-side regression tests, including the new PS2 IMG/MDF scanner and webMAN route tests: **PASS**
- Arabic bitmap preview render: **PASS**
- Staircase material/motion preview render: **PASS**
- PS3 ELF link: **PASS**
- CEX SELF generation: **PASS**
- Standard PKG generation: **PASS**
- GNPDRM PKG generation and package checks: **PASS**
- Embedded XMB artwork byte-for-byte verification (`ICON0.PNG`, `PIC0.PNG`, `PIC1.PNG`): **PASS**
- Embedded Apollo Save logo, Music 1 source and Music 2 theme: **PASS**

Run: https://github.com/Vikik0/PS3-SCALE/actions/runs/44963475576

## Build artifact

Artifact name: `PS3-SCALE-Alpha-0.28-build-with-embedded-theme`

Contains:
- `ps3scale.elf` — 3,504,173 bytes
- `ps3scale.self` — 779,280 bytes
- `ps3scale.pkg` — 11,402,352 bytes
- `ps3scale.gnpdrm.pkg` — 11,402,352 bytes

SHA-256:
- ELF: `ea70b72be8e36171a526bf6331a38a920b742b9a0942f44e5ec18a605a40a128`
- SELF: `bb4c38d211b6800554f7855803dcbfa31bc1a7ecf1e0d8d178f506e7b0527353`
- Standard PKG: `18a35d68a448afc4cf656e8c87537d21aae168ce8c108dede39db4d9857bf34e`
- GNPDRM PKG: `66b6c976afd36e833952f1974b6c2909acdc0d14a7cbabb80e9481b7d5a76cac`
- Build ZIP: `a8cf24472c8adbf99f5546f911f957360f506fac7361d661535565f980e6a1eb`

## Alpha 0.23 changes in this continuation

- Removed the FEATURES heading from Home and removed the duplicate image panel from About. The XMB `ICON0.PNG`, `PIC0.PNG` and `PIC1.PNG` use the approved pearl/lavender staircase identity.
- Renamed the visible playlist entries to Music 1 and Music 2.
- Added the Enhancement Storage setting: Temporary by default or Permanent for new Artemis NCL files.
- Online DB now refreshes automatically on first open, uses Triangle for manual refresh and Select for search, and defaults to Graphics + Any Region/Version.
- Added strict Title ID + APP_VER matching for community installs; a same-title/same-region candidate is browse-only and cannot authorize a download.
- Added exact-match `GRAPHICS_FPS` NCL installation to the detected Artemis USERLIST directory. Both upstream `ARTPS3001` and Artemis R5 `ARTZ00001` package IDs are supported. The app validates the source path, exact Title ID + APP_VER, text format and size, and never overwrites an existing file.
- Added an exact byte-size check against the refreshed community index. If the upstream NCL changes after indexing, the download is discarded and the user is asked to refresh the Online DB.
- Temporary NCL files are tracked and removed on the next PS3 SCALE start. Permanent files remain until manually removed. PS3 SCALE does not execute or auto-apply NCL files; the user selects them in Artemis.
- Replaced the synthetic Game Details comparison illustration with an exact-version local before/after PNG loader. No genuine game screenshot pairs are bundled yet; the UI reports a missing pair instead of showing a concept image.
- Fixed webMAN local HTTP initialization by loading NET/NETCTL and calling `netCtlInit()`. Added distinct launch error messages.
- Fixed the PS3MAPI availability probe to read and validate the complete JSON response body rather than relying on the headers-only packet. This avoids false "MAPI offline" results when HTTP headers and body arrive separately.
- If PS3MAPI is unavailable after a PS3 SCALE profile was armed, the app removes its own one-launch script and still attempts an unmodified game launch.
- Updated package SFO metadata to 00.23 and aligned GitHub Actions artifact names.

## Alpha 0.24 changes in this continuation

- Replaced the generated placeholder XMB icon with the approved square PS3 SCALE Neon Glass image, centered without stretching in the 320×176 ICON0 canvas.
- Removed the extra decorative rings around the in-app spiral staircase; the Home scene now renders one staircase only.
- Online DB defaults to All Files + Any Region/Version and lists unmatched community entries as browse-only immediately on first open. Triangle still forces a live refresh.
- CROSS/START in Game Details now arms an exact one-write B_COMMUNITY memory profile and launches the game. No exact profile means the original game starts unmodified; manual and multi-write profiles remain blocked.
- Kept the real Before/After screenshot loader: no synthetic green preview is shown. Genuine captures are still required per Title ID + APP_VER.

## Alpha 0.25 XMB correction

- Alpha 0.24 updated `ICON0.PNG`, but the large selected-item artwork could still show the older staircase-and-text `PIC0.PNG`, making the main image look unchanged on the console.
- Alpha 0.25 now generates both `ICON0.PNG` and `PIC0.PNG` from the same approved Neon Glass square logo. The old generated staircase/text panel is removed from `PIC0.PNG`.
- `PIC1.PNG` remains the separate full-screen pearl/lavender wallpaper with one staircase.
- Package verification compares every embedded XMB PNG byte-for-byte against the freshly generated file, as well as checking PNG dimensions.
- Package version is bumped to `00.25` to ensure the PS3 receives a distinct update build.

## Remaining verification

- **No physical PS3 installation or game launch was performed by CI.** Install the GNPDRM PKG on the target CFW console and test controller navigation, scan, Online DB refresh, Artemis USERLIST loading, temporary/permanent cleanup, webMAN launch, and one safe memory profile.
- Real before/after screenshots still need to be captured and supplied for each supported Title ID + APP_VER. The loader is implemented, but screenshot coverage is not complete.
- Community NCL files and memory profiles remain community-supplied. Exact matching, index-size validation and text checks do not prove that a patch is safe or hardware-tested.
- Remaining compiler/configure warnings are from bundled third-party mpg123 / PS3 network dependency sources (for example, unused parameters, signedness and optional libpsl/IDN/manual support). The PS3 application source builds successfully.

Do not merge Draft PR #16 until the physical-console smoke test and review are complete.