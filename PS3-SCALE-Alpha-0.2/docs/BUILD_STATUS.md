# PS3 SCALE Alpha 0.2 — Build status

Date: 2026-11-08

## Latest GitHub Actions build
- Workflow run: [#92 — successful](https://github.com/Vikik0/PS3-SCALE/actions/runs/42485983279)
- Source commit: `96319a5d4d169b3f187bc78d39480035e9881230`
- Artifact: [PS3-SCALE-Alpha-0.2-build](https://github.com/Vikik0/PS3-SCALE/actions/runs/42485983279/artifacts/13619531282)
- Artifact ID: `13619531282`
- Artifact ZIP SHA-256: `2108969ba7ef080108c04afd847c29ab585a8a801cbf7b27f6e68c06d877b526`

## Build outputs

| File | Size | SHA-256 |
|---|---:|---|
| `ps3scale.elf` | 1,524,928 bytes | `2d88cac45011e5472d2f45a84a5d41fad8f2b5f4550d1ca112d40dabe01003f9` |
| `ps3scale.self` | 172,960 bytes | `438d4edbf697d449d14ee451001fad1c5068d85d47afad17aee5d57ab536f2d8` |
| `ps3scale.pkg` | 181,712 bytes | `94d6c3f0c9abc3675d325037cbb4b6f87351dce6948d890aa4e821d4dee20dc0` |
| `ps3scale.gnpdrm.pkg` | 181,712 bytes | `794b6f6678e85d08685f47665890fce0f2512580ff25a968ce04e06dff48a165` |

## Package validation
- PKG magic: `7f504b47`
- Finalized package revision: `8000`
- Package type: `0001` (PS3)
- Content ID: `UP0001-PS3SCALE1_00-0000000000000000`

The package passed the automated structural checks. This does not replace installation and runtime verification on the console.

## Tests and current features
- Host-side SFO parser tests: PASS
- Host-side scanner tests, including local cover discovery: PASS
- Host-side ISO metadata tests: PASS
- Host-side PS2 CONFIG builder tests: PASS
- PPU ELF / CEX SELF / debug PKG / finalized NPDRM PKG: built successfully
- Main menu with native Tiny3D vector icons: Scan Games (magnifier), Settings (gear), About (information)
- Separate PS3 and PS2 scan views
- PS3 system on-screen keyboard title search
- Game details page with identity and source path
- Local ICON0.PNG discovery and YA2D rendering for folder/installed titles
- PS2 CONFIG preview only; no sidecar file is written or applied
- Streaming MP3 decoder (LGPL mpg123) linked with native PSL1GHT audio output
- Startup music auto-start, loop, mute toggle and volume controls implemented
- MP3 asset is not embedded in the PKG; the app checks internal USRDIR and USB000–USB007 paths

## Console smoke test reported by user
A previous Alpha build launched successfully on the target CECH-2504A / CEX CFW 4.93 / Cobra 8.5 console. The user supplied photos showing:
- Main menu navigation screen
- PS3 game scan list with detected titles
- Game details page and local Minecraft ICON0 cover

Run 92 has passed CI but has not yet been installed on the console. The new vector icons and audio playback therefore still need a physical-console check.

## Remaining validation
1. Install the Run 92 package over the existing app (same package identity) and confirm the three vector icons render.
2. Copy the supplied music folder `PS3SCALE/theme.mp3` to the root of a FAT32 USB drive and keep it connected; the app searches USB000 through USB007. Alternatively copy the file to `/dev_hdd0/game/PS3SCALE1/USRDIR/theme.mp3`.
3. Confirm music starts automatically, continues across screens, loops, and responds to mute/volume.
4. Confirm audio shuts down cleanly when exiting the app.
5. Confirm Settings and About open and return correctly.
6. Test PS3/PS2 library switching and SELECT search using the controller.
7. Confirm local covers render correctly on the CECH-2504A.
8. Test PS2 CONFIG preview; confirm it does not write any game file.
9. Confirm no original game files or PARAM.SFO files changed.

## Not implemented yet
The actual verified graphics-profile workflow is still pending: online catalog/download, exact Title ID + update matching, backup/rollback, a CFW runtime plugin, the post-apply PS3 beep, and the in-game `Graphics applied — Enjoy!` notification. No unsupported game files are changed and no success notification is faked.
