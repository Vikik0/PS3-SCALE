# PS3 SCALE Alpha 0.2 — Build status

Date: 2026-11-08

## Latest GitHub Actions build
- Workflow run: [#70 — successful](https://github.com/Vikik0/PS3-SCALE/actions/runs/42484129338)
- Source commit: `8450929cbd30f597fb022c780eb76dcecba7690d`
- Artifact: [PS3-SCALE-Alpha-0.2-build](https://github.com/Vikik0/PS3-SCALE/actions/runs/42484129338/artifacts/13618701383)
- Artifact ID: `13618701383`
- Artifact ZIP SHA-256: `cf2a31fc754d1c25e6f7397b6f50112158691ff776d58913f7d16142c42729ca`

## Build outputs

| File | Size | SHA-256 |
|---|---:|---|
| `ps3scale.elf` | 1,268,086 bytes | `c59e44b7455ec5b7713c906d297e3f766f60065687476a2c8d94d415031fa59a` |
| `ps3scale.self` | 105,200 bytes | `e7ff21cfc9eedcee1c21a2f97800546661112db67c4db9d827d35ac15e90872f` |
| `ps3scale.pkg` | 113,936 bytes | `bb33bc7bd06f6a015b472f13131d5b7e658f7a8d4028f8f1b77cf5cc9f5be638` |
| `ps3scale.gnpdrm.pkg` | 113,936 bytes | `1e59e37b4200275d511f45bd0fc32e31a30e3ec9ac3839ec355a7938e16a0e1d` |

## Package validation
- PKG magic: `7f504b47`
- Finalized package revision: `8000`
- Package type: `0001` (PS3)
- Declared total size matches actual size: 113,936 bytes
- Content ID: `UP0001-PS3SCALE1_00-0000000000000000`

The finalized package passed the automated structural checks. This does not replace installation and runtime verification on the console.

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
- Startup MP3 playback and runtime graphics plugin are not yet implemented

## Console smoke test reported by user
On the target CECH-2504A / CEX CFW 4.93 / Cobra 8.5 console, the previous Alpha build launched and the user supplied photos showing:
- Main menu navigation screen
- PS3 game scan list with detected titles
- Game details page and local Minecraft ICON0 cover

The newly built Run 70 icon update has not yet been visually checked on the console.

## Remaining validation
1. Install the Run 70 package over the existing app (same package identity) and confirm the three vector icons render.
2. Confirm Settings and About open and return correctly.
3. Test PS3/PS2 library switching and SELECT search using the controller.
4. Confirm local covers render correctly on the CECH-2504A.
5. Test PS2 CONFIG preview; confirm it does not write any game file.
6. Confirm no original game files or PARAM.SFO files changed.
7. Capture updated console screenshots and record actual results.
