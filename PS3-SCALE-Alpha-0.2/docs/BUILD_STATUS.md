# PS3 SCALE Alpha 0.2 — Build status

Date: 2026-11-09

## Latest feature-branch CI build

- Workflow run: [#215 — successful](https://github.com/Vikik0/PS3-SCALE/actions/runs/42534237391)
- Pull request: [#6 — draft, not merged](https://github.com/Vikik0/PS3-SCALE/pull/6)
- Source branch: feature/online-graphics-database
- Source commit: 633d8e7326c0f88c8a90e64a428ee0e643da2a5c
- Artifact: PS3-SCALE-Alpha-0.2-build-with-embedded-theme
- Artifact ID: 13638538498
- Artifact ZIP size: 14,172,537 bytes
- Artifact ZIP SHA-256: c27d4f5e61569ccbabf01eec8b9707806feb97db43fde3d30b1bfd2045853e79

## Build outputs

| File | Size | SHA-256 |
|---|---:|---|
| ps3scale.elf | 3,413,234 bytes | 578de0b83eaf87ee6508fd615f896f59ee0d7f0d632d4dc9bf3080514c776479 |
| ps3scale.self | 727,696 bytes | 0a4231ba290aa54ed1bcc46ce6a32751133a41185894e3bc85ad347ca9ac765f |
| ps3scale.pkg | 6,198,048 bytes | 7717fc859e797fbe8251260c71e8bf208c664cc25c152ea4062988542b02842b |
| ps3scale.gnpdrm.pkg | 6,198,048 bytes | dac5ad6f66896cece80d0a88167cbaf37b5cf72620ce587d1fc5be66c7bb33dc |

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

## Implemented in Run #215

- First-run language selection: English, Arabic and Spanish; saved in USRDIR/settings.cfg.
- Spanish translations and an embedded compact Arabic bitmap glyph atlas derived from Noto Sans Arabic.
- Focused Arabic contextual letter shaping and right-to-left run ordering for translated UI strings.
- In-app controller keyboard with English, Spanish and Arabic layouts; SELECT and START open it. Native PS3 OSK is no longer used.
- UTF-8 search input and code-point-aware backspace.
- Staged startup/loading screen with local app-file check, companion-tool detection and worker-thread game-library scan.
- Slow low-poly 3D wireframe home emblem with two orbit rings; can be disabled in Settings.
- Settings persist language, music ON/OFF, volume and animation ON/OFF. Music OFF skips audio initialization at startup and shuts down the decoder, audio port and thread immediately; Music ON restarts them.
- Reset Settings restores English, Music ON, default volume and Home Animation ON.
- Online catalogue refresh is deferred until the user opens Online Graphics Database; normal startup and game launching remain offline.
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

The 40 starter entries comprise 33 single-write B_COMMUNITY profiles eligible for one-launch confirmation, 6 multi-write B_COMMUNITY profiles disabled for automation, and 1 B_COMMUNITY_MANUAL profile (Brütal Legend) excluded because the source requires main-menu application and reapplication after the game resets it. All are community-reported and not independently hardware-verified by PS3 SCALE. A successful CI build verifies compilation and host-side contracts, not that a specific profile works on every console or game revision.

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
