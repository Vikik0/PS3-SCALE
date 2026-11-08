# PS3 SCALE Alpha 0.2 — Build status

Date: 2026-11-08

## Latest verified CI build
- Workflow run: [ #105 — successful](https://github.com/Vikik0/PS3-SCALE/actions/runs/42488954976)
- Source commit: `9c7638de31db624af4abc4673718891cf3d553f5`
- Artifact ID: `13621056476`
- Artifact ZIP SHA-256: `31d25b7b4f350debf7033a44f2d6590f8cd26e2dfcc75325c845ffeb6d0481c4`

## Build outputs
| File | Size | SHA-256 |
|---|---:|---|
| `ps3scale.elf` | 1,642,594 bytes | `67f09e899dec5de547790688d33995993315bf4a79aa18b1985fe1af5d1e2e6a` |
| `ps3scale.self` | 180,288 bytes | `2e971e24a1244b36cb500cc09bc1fac3787ecdbd97d50fc6268b1dd17337d64e` |
| `ps3scale.pkg` | 221,776 bytes | `7dfc823e251aab51a87e271c6ed4956592d13af67097060fffb8ddb56690ae8b` |
| `ps3scale.gnpdrm.pkg` | 221,776 bytes | `ee91bcd185e8be2f9e975d9faab7131d1846f0f7db120791e0d06ef25a972065` |

The finalized PKG header is `7f504b47 8000 0001` (PS3 package, finalized revision).

## Tests
- Host SFO parser tests: PASS.
- Host scanner tests, including local cover discovery: PASS.
- Host ISO metadata tests: PASS.
- Host PS2 CONFIG builder tests: PASS.
- Static UI contract tests: PASS.
- PPU ELF / CEX SELF / debug PKG / finalized NPDRM PKG: built successfully.

## Implemented in the current UI revision
- Main menu: Scan Games, Settings, Companion Tools, About.
- Slow Tiny3D vector-orbit animation with an OFF setting; only a few primitives are drawn per frame.
- Library overview panel with PS3, PS2 and total counts.
- Separate PS3/PS2 scan lists sorted by title.
- Smaller scan-screen font and independent columns for list, selected-game metadata and cover art.
- Native PS3 OSK search lifecycle corrected: text is collected with `oskGetInputText` and OSK events use a separate sysutil callback slot.
- Settings persist Music ON/OFF, volume and home animation in `USRDIR/settings.cfg`.
- Settings actions: offline USB update-package discovery, library rescan, restore defaults and copy the music MP3 from USB to internal USRDIR.
- Muting music outputs silence without continuing MP3 decoding.
- Companion Tools page documents Apollo Save Tool, Artemis PS3 and webMAN MOD as separate applications; no third-party plugins or data are modified.

## Music behavior
The MP3 is not embedded in the PS3 firmware or in the current PKG. The application searches internal USRDIR first, then the USB music folder. Settings includes **COPY MUSIC TO INTERNAL HDD**. After copying, restart PS3 SCALE; it will use the internal copy and the USB can be removed.

## Console validation
The user has confirmed that a previous build launches, scans the actual PS3/PS2 library, renders local cover art, shows the Settings screen and plays the supplied GTA V remix. Run #105 has passed CI but the new layout, OSK fix, persistent settings, copy-to-HDD action and slow animation still need a physical-console smoke test.

## Remaining product work
The actual verified graphics-profile workflow is not complete: signed online catalog, exact Title ID + update matching, profile compatibility database, backup/rollback, CFW runtime plugin, and post-apply in-game notification. The USB update check is an offline file discovery only; it does not verify a release signature or auto-install. No profile is marked applied and no original game file is modified.
