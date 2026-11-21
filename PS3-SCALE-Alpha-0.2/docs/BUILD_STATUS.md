# Build status — PS3 SCALE Alpha 0.25

## Current development head

- Branch: `design/splash-transitions-ui-v6`
- Pull request: Draft PR #16, targeting `design/settings-support-ui-v5`
- Application version shown in the UI: `ALPHA 0.25`
- Package metadata: `APP_VER=00.25`, `VERSION=00.25`
- Package title ID: `PS3SCALE1`

## Latest code/package CI validation

GitHub Actions run **#555** completed successfully on 2026-11-21.

- Host-side regression tests: **PASS**
- Arabic bitmap preview render: **PASS**
- Staircase material/motion preview render: **PASS**
- PS3 ELF link: **PASS**
- CEX SELF generation: **PASS**
- Standard PKG generation: **PASS**
- GNPDRM PKG generation and package checks: **PASS**
- Embedded XMB artwork byte-for-byte verification (`ICON0.PNG`, `PIC0.PNG`, `PIC1.PNG`): **PASS**
- Embedded Apollo Save logo, Music 1 source and Music 2 theme: **PASS**

Run: https://github.com/Vikik0/PS3-SCALE/actions/runs/44951980356

## Build artifact

Artifact name: `PS3-SCALE-Alpha-0.25-build-with-embedded-theme`

Contains:
- `ps3scale.elf` — 3,503,574 bytes
- `ps3scale.self` — 777,760 bytes
- `ps3scale.pkg` — 11,400,816 bytes
- `ps3scale.gnpdrm.pkg` — 11,400,816 bytes

SHA-256:
- ELF: `f78ef93b3b79a388e403d805b570129ac7d5753ec1848988d5174c171bc5c5a2`
- SELF: `c6e7aa033a82c2103d22a9b548bb28c778afe8f79dcabff31b75e5844bcf900f`
- Standard PKG: `e24e8cc9f421d464ddd190ac7cfc81cc6152c19af9e20f941b5152550e863fab`
- GNPDRM PKG: `df9a9da2498f8ec672ec884c1cd04495912530462c6da34a759b065d67f7077f`

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
