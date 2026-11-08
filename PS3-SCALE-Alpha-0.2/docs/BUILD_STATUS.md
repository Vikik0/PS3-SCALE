# PS3 SCALE Alpha 0.2 — Build status

Date: 2026-11-08

## Latest verified CI build

- Workflow run: [#119 — successful](https://github.com/Vikik0/PS3-SCALE/actions/runs/42494932138)
- Source commit: `7a984e43c279d33c5e5056883c27b21b40a5870f`
- Artifact ID: `13622478698`
- Artifact ZIP size: 1,203,134 bytes
- Artifact ZIP SHA-256: `3be24fe4d3b93cc8dd2f538da2c1ea1aaa9995e216a9f3a52d64a7f2044705ee`

## Build outputs

| File | Size | SHA-256 |
|---|---:|---|
| `ps3scale.elf` | 1,646,258 bytes | `d57b00e2e86983dc1d7e38099890acd788e8ced6426b416b809b2d6162b73f5c` |
| `ps3scale.self` | 220,544 bytes | `185b2e9e010dced8736c97089218f215bc2f78248e87791c8980cf8bb4c47573` |
| `ps3scale.pkg` | 262,032 bytes | `aeacb3af5aa427f3d3406235647d45c33ab39bfd29ebd6ce82251a6da7d68bb2` |
| `ps3scale.gnpdrm.pkg` | 262,032 bytes | `8a0f86d8bfaa139d5828b80336196bea55f15e2d0200b92e420b4a0cdc301add` |

The finalized package header is `7f504b47 8000 0001` (PS3 package, finalized revision).

## CI tests

- Host SFO parser tests: PASS.
- Host scanner tests: PASS.
- Host ISO metadata tests: PASS.
- Host PS2 CONFIG builder tests: PASS.
- Host locale / UTF-8 glyph mapping / keyboard layout tests: PASS.
- Static UI contract tests: PASS.
- PPU ELF, CEX SELF, debug PKG and finalized NPDRM PKG: built successfully.

## Implemented in Run #119

- First-run language selection: English, Arabic and Spanish; saved in `USRDIR/settings.cfg`.
- Spanish translations and an embedded compact Arabic bitmap glyph atlas derived from Noto Sans Arabic.
- Focused Arabic contextual letter shaping and right-to-left run ordering for translated UI strings.
- In-app controller keyboard with English, Spanish and Arabic layouts; SELECT and START open it. Native PS3 OSK is no longer used.
- UTF-8 search input and code-point-aware backspace.
- Staged startup/loading screen with local app-file check, companion-tool detection and worker-thread game-library scan.
- Slow low-poly 3D wireframe home emblem with two orbit rings; can be disabled in Settings.
- Settings persist language, music, volume and animation.
- Existing scanner, cover-art layout, companion-tool discovery, music streaming and USB-to-HDD music copy remain in the build.

## Localization scope

Arabic rendering is tailored to the app's translated interface and common Arabic letters. This is not a full Unicode shaping/bidi stack; advanced mixed-direction cases, diacritics and unsupported characters in arbitrary game titles may fall back or render imperfectly. See `docs/LOCALIZATION_AND_STARTUP.md` and `docs/THIRD_PARTY_LICENSES.txt`.

## Music

The user-supplied MP3 is not embedded inside the PKG. The app reads internal USRDIR first, then the original filename in USRDIR, then `PS3SCALE/theme.mp3` on USB000–USB007. Settings can copy the music to internal HDD.

## Update and download policy

Startup performs local checks only. It does not need internet access and does not download arbitrary files. The USB update check only discovers a PKG with PS3 package magic; it does not verify a signed release manifest or auto-install. A secure online updater remains future work.

## Physical console validation

A previous Alpha build was confirmed by the user to launch, scan the library, display cover art and play the supplied music. Run #119 passes host CI and builds the final PKG, but the new Arabic glyph rendering, language persistence, custom keyboard navigation, 3D frame pacing, startup scan behavior and audio still need a fresh physical-console smoke test on the target PS3. Host CI does not replace hardware validation.

## Not implemented

- No verified online graphics-profile catalog or automatic patch download/application.
- No game-file backup/rollback workflow because the current app does not modify game files.
- No CFW runtime plugin/PRX for game-start patching.
- No in-game "Graphics applied — Enjoy!" notification or post-apply beep.
- No universal AI upscaler or frame generation.
- USB update discovery does not verify a signed release manifest and does not install packages automatically.
