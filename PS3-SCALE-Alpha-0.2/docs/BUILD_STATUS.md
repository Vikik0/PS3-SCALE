# PS3 SCALE Alpha 0.2 — Build status

Date: 2026-11-09

## Latest feature-branch CI build

- Workflow run: [#178 — successful](https://github.com/Vikik0/PS3-SCALE/actions/runs/42532501388)
- Pull request: [#6 — draft, not merged](https://github.com/Vikik0/PS3-SCALE/pull/6)
- Source branch: feature/online-graphics-database
- Source commit: 8c2bd97987583f1fc9f448cbc401d8a1c8af87ed
- Artifact: PS3-SCALE-Alpha-0.2-build-with-embedded-theme
- Artifact ID: 13638236345
- Artifact ZIP size: 14,168,053 bytes
- Artifact ZIP SHA-256: 113ab4f953c8e291cb46e1873dad5b0e1cd05e9836e6bb998d68a5d957ac5c70

## Build outputs

| File | Size | SHA-256 |
|---|---:|---|
| ps3scale.elf | 3,413,161 bytes | c2076ce6918320672cdf80f6dfa420680ca07a6429babf737c4966e1680d5fc4 |
| ps3scale.self | 726,544 bytes | f928cf545f321e131729a519e2997cc9965a7b2ab5b1d64527632509117b23a2 |
| ps3scale.pkg | 6,196,832 bytes | 93957593d155ed85cbc91fbef8dc4d1f341773464489872f5a848f9109ee2d45 |
| ps3scale.gnpdrm.pkg | 6,196,832 bytes | fd2f68a7e0ba84010512b75e9919628629db88d3bf772357ae7a3984fffcd625 |

Package headers:
- Debug PKG: 7f504b47 00000001
- GNPDRM PKG: 7f504b47 80000001

The GNPDRM package is the intended install package for the user's CFW console. The ZIP artifact contains both packages plus the ELF and SELF build outputs. The artifact and extracted package headers and SHA-256 values were checked after download.

## CI tests

- Host SFO parser tests: PASS.
- Host scanner tests: PASS.
- Host ISO metadata tests: PASS.
- Host PS2 CONFIG builder tests: PASS.
- Host catalog parsing, duplicate detection and invalid patch rejection tests: PASS.
- One-launch webMAN script tests: PASS, including aligned/unaligned address guards, V1 stale-script removal, full-body tamper detection/repair and preservation of user-owned scripts.
- Host locale / UTF-8 glyph mapping / keyboard layout tests: PASS.
- Static UI contract tests: PASS.
- PPU ELF, CEX SELF, debug PKG and finalized GNPDRM PKG: built successfully.
- Embedded theme assembly and asset integrity check: PASS.

## Implemented in Run #178

- First-run language selection: English, Arabic and Spanish; saved in USRDIR/settings.cfg.
- Spanish translations and an embedded compact Arabic bitmap glyph atlas derived from Noto Sans Arabic.
- Focused Arabic contextual letter shaping and right-to-left run ordering for translated UI strings.
- In-app controller keyboard with English, Spanish and Arabic layouts; SELECT and START open it. Native PS3 OSK is no longer used.
- UTF-8 search input and code-point-aware backspace.
- Staged startup/loading screen with local app-file check, companion-tool detection and worker-thread game-library scan.
- Slow low-poly 3D wireframe home emblem with two orbit rings; can be disabled in Settings.
- Settings persist language, music ON/OFF, volume and animation ON/OFF.
- The supplied MP3 is assembled into USRDIR/theme.mp3 inside the PKG; no USB copy-to-HDD action is needed.
- PS3/PS2 library scanning, PARAM.SFO metadata, cover layout, companion-tool discovery and webMAN Start Game support.
- Online catalogue with 40 starter PS3 profiles, HTTPS verification, 64 KiB response cap, strict parsing and safe cache replacement.
- Exact Title ID + APP_VER profile matching; profile data is not selected by title name alone.
- One-launch webMAN scripts for single-write B_COMMUNITY profiles after explicit confirmation. Script headers fingerprint Title ID, APP_VER and patch data.
- Runtime check of Title ID, in-game state, exact memory address and expected original bytes. The search step is constrained to one candidate address.
- Before Start Game, PS3 SCALE compares the complete V2 script body with the deterministic expected script. A matching header alone is insufficient; manually edited or appended commands are treated as stale.
- Legacy V1 and stale PS3 SCALE scripts are detected; user-owned webMAN scripts are never overwritten or deleted.
- Multi-write profiles remain disabled for automatic application. The one-launch script deletes itself after the launch attempt.
- No success popup is shown because the current webMAN batch path cannot verify the post-write memory value.

## Profile trust and limitations

All 40 starter catalogue entries are B_COMMUNITY: community-reported and not independently hardware-verified by PS3 SCALE. A successful CI build verifies compilation and host-side contracts, not that a specific profile works on every console or game revision.

The webMAN batch language does not expose a direct APP_VER condition. PS3 SCALE checks APP_VER before arming and against the mounted game when Start Game is used. Do not update a game between arming and launching the profile. The patch changes only the running game process memory; original game files remain unchanged. Exiting the game clears the process-memory patch.

No universal AI upscaler or frame generation is provided, and no fixed 60 FPS result is guaranteed.

## Physical console validation

The target device information currently available is PS3 Slim with CFW 4.92; the exact CECH model, CEX/DEX type and Cobra version are not yet confirmed. A previous Alpha build was confirmed by the user to launch, scan the library, display cover art and play the supplied music. **This feature-branch package has not yet been installed or smoke-tested on the physical console.**

Recommended first hardware check: install the GNPDRM PKG, verify launch/library scan/music/settings, then test only one reversible single-write profile after confirming the exact game Title ID and APP_VER. Keep the original game files untouched and exit the game to clear the in-memory patch.

## Not implemented

- No universal AI upscaler, frame generation or guaranteed 60 FPS.
- No automatic multi-write profile application.
- No post-write memory verification or success popup.
- No game-file backup/rollback workflow because this implementation does not modify game files.
- No signed online updater or automatic package installation; USB update discovery remains a separate local feature.
