# Build status — PS3 SCALE Alpha 0.23

## Current development head

- Branch: `design/splash-transitions-ui-v6`
- Pull request: Draft PR #16, targeting `design/settings-support-ui-v5`
- Application version shown in the UI: `ALPHA 0.23`
- Package metadata: `APP_VER=00.23`, `VERSION=00.23`
- Package title ID: `PS3SCALE1`

## Latest CI validation

GitHub Actions run **#534** completed successfully on 2026-11-21.

- Host-side regression tests: **PASS**
- Arabic bitmap preview render: **PASS**
- Staircase material/motion preview render: **PASS**
- PS3 ELF link: **PASS**
- CEX SELF generation: **PASS**
- Standard PKG generation: **PASS**
- GNPDRM PKG generation and package checks: **PASS**
- Embedded Apollo Save logo, Music 1 source and Music 2 theme: **PASS**

Run: https://github.com/Vikik0/PS3-SCALE/actions/runs/44941356426

## Build artifact

Artifact name: `PS3-SCALE-Alpha-0.23-build-with-embedded-theme`

Contains:
- `ps3scale.elf` — 3,503,492 bytes
- `ps3scale.self` — 777,232 bytes
- `ps3scale.pkg` — 11,395,872 bytes
- `ps3scale.gnpdrm.pkg` — 11,395,872 bytes

SHA-256:
- ELF: `17c56decbccc664ba745c87bca10d90431a1c20ffb8b3fc9cf540b01cf4de6ef`
- SELF: `c94b672412f2391b041ca33da2a2cbbdc755ecd31fdfd15ffdf2887d3e75e672`
- Standard PKG: `b5e9068cb82d4d9e7543a40453433e83f2bbab74da513f14ecb0970e6445214a`
- GNPDRM PKG: `de39abea7efaaafa5454c240f96134cc18321bd4d48f675f0a0a7bf1da01b516`

## Alpha 0.23 changes in this continuation

- Removed the FEATURES heading from Home and removed the duplicate image panel from About. The XMB `ICON0.PNG`, `PIC0.PNG` and `PIC1.PNG` use the approved pearl/lavender staircase identity.
- Renamed the visible playlist entries to Music 1 and Music 2.
- Added the Enhancement Storage setting: Temporary by default or Permanent for new Artemis NCL files.
- Online DB now refreshes automatically on first open, uses Triangle for manual refresh and Select for search, and defaults to Graphics + Any Region/Version.
- Added exact-match `GRAPHICS_FPS` NCL installation to the detected Artemis USERLIST directory. Both upstream `ARTPS3001` and Artemis R5 `ARTZ00001` package IDs are supported. The app validates the source path, exact Title ID + APP_VER, text format and size, and never overwrites an existing file.
- Temporary NCL files are tracked and removed on the next PS3 SCALE start. Permanent files remain until manually removed. PS3 SCALE does not execute or auto-apply NCL files; the user selects them in Artemis.
- Replaced the synthetic Game Details comparison illustration with an exact-version local before/after PNG loader. No genuine game screenshot pairs are bundled yet; the UI reports a missing pair instead of showing a concept image.
- Fixed webMAN local HTTP initialization by loading NET/NETCTL and calling `netCtlInit()`. Added distinct launch error messages. If PS3MAPI is unavailable after a PS3 SCALE profile was armed, the app removes its own one-launch script and still attempts an unmodified game launch.
- Updated package SFO metadata to 00.23 and aligned GitHub Actions artifact names.

## Remaining verification

- **No physical PS3 installation or game launch was performed by CI.** Install the GNPDRM PKG on the target CFW console and test controller navigation, scan, Online DB refresh, Artemis USERLIST loading, temporary/permanent cleanup, webMAN launch, and one safe memory profile.
- Real before/after screenshots still need to be captured and supplied for each supported Title ID + APP_VER. The loader is implemented, but screenshot coverage is not complete.
- Community NCL files and memory profiles remain community-supplied. Exact matching and text validation do not prove that a patch is safe or hardware-tested.
- Remaining compiler/configure warnings are from bundled third-party mpg123 / PS3 network dependency sources (for example, unused parameters, signedness and optional libpsl/IDN/manual support). The PS3 application source builds successfully.

Do not merge Draft PR #16 until the physical-console smoke test and review are complete.
