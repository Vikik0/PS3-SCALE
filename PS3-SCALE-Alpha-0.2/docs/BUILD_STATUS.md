# PS3 SCALE Alpha 0.2 — Build status

Date: 2026-11-08

## Latest code build

- Workflow run: [#115 — successful](https://github.com/Vikik0/PS3-SCALE/actions/runs/42491128350)
- Source commit: \`16c70450996626dbcd12890231fcab2f0443b41f\`
- Artifact ID: \`13621377066\`
- Artifact ZIP size: 1,044,424 bytes
- Artifact ZIP SHA-256: \`eb8882e3f700226759ddd92793eadbd6cf025f19d278fae464463929e92b65aa\`

## Build outputs

| File | Size | SHA-256 |
|---|---:|---|
| \`ps3scale.elf\` | 1,643,015 bytes | \`e4403c837fb498627f716299cf11f9263133382051c786720407b88c5285dbc3\` |
| \`ps3scale.self\` | 181,456 bytes | \`4b316495b9d8d3a9c0a307a7e6f5d9c45a8aabcdbf736dd1745aa6d75d098b60\` |
| \`ps3scale.pkg\` | 222,928 bytes | \`4cb06cf1a809f6cee255d50aee9ca6cea475c0691543ef07a73b4a7b9dd6fcab\` |
| \`ps3scale.gnpdrm.pkg\` | 222,928 bytes | \`e4edc12a2d0b81b43da11b7ebc2510f7a8768e149cb9a1044ef2faa4c508ab7b\` |

The final package starts with PS3 PKG magic \`7f504b47\`; the CI package validation step completed successfully.

## CI tests

- Host SFO parser tests: PASS.
- Host scanner tests: PASS.
- Host ISO metadata tests: PASS.
- Host PS2 CONFIG builder tests: PASS.
- Static UI contract tests: PASS.
- PPU ELF, CEX SELF, debug PKG and finalized NPDRM PKG: built successfully.

## Implemented in Run #115

- Main menu with four destinations, vector icons, library overview and a slow lightweight ambient animation.
- Animation can be disabled from Settings.
- Scan Games now uses two fixed non-overlapping panels, smaller font, shortened list type labels and a separated cover-art region.
- SELECT and START both open the search OSK; the initial full-width alphabet panel is now included in the allowed OSK panel mask.
- Settings persist music, volume and animation state.
- Copy-to-HDD music action restarts the audio backend to switch to the internal copy, including when the file already exists.
- Offline USB update-file discovery, library rescan and restore defaults.
- Companion Tools page checks for Apollo Save Tool, Artemis PS3 and webMAN MOD using known local file paths; it does not load or modify their plugins or data.

## Music

The user-supplied MP3 is not embedded inside the PKG. The USB bundle keeps it as \`PS3SCALE/theme.mp3\`; the app can copy it to \`/dev_hdd0/game/PS3SCALE1/USRDIR/theme.mp3\`. The bundle includes the finalized PKG and a SHA-256 manifest.

## Physical console validation

A previous Alpha build was confirmed by the user to launch, scan the library, display cover art and play the music. Run #115 passes CI, but its revised scan layout, SELECT/START keyboard, tool detection and immediate music handoff still require a fresh physical-console smoke test.

## Not implemented

- No verified online graphics-profile catalog or automatic patch download/application.
- No game-file backup/rollback workflow because the current app does not modify game files.
- No CFW runtime plugin/PRX for game-start patching.
- No in-game "Graphics applied — Enjoy!" notification or post-apply beep. A standalone GameOS manager exits when the game launches; a separate runtime component must validate the running title and apply the matching patch.
- No universal AI upscaler or frame generation.
- USB update discovery does not verify a signed release manifest and does not install packages automatically.
