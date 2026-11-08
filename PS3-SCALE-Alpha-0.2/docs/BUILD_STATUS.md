# PS3 SCALE Alpha 0.2 — Build status

Date: 2026-11-08

## Latest GitHub Actions build
- Workflow run: [#84 — successful](https://github.com/Vikik0/PS3-SCALE/actions/runs/42485612038)
- Source commit: `f5642796325333ee8c5af44d1295f76657998c48`
- Artifact: [PS3-SCALE-Alpha-0.2-build](https://github.com/Vikik0/PS3-SCALE/actions/runs/42485612038/artifacts/13619530789)
- Artifact ID: `13619530789`
- Artifact ZIP SHA-256: `59899713ad164eca6c398e56e07f0d3364d6589c153a5672b9e94bb62ad3ff2f`

## Build outputs

| File | Size | SHA-256 |
|---|---:|---|
| `ps3scale.elf` | 1,524,928 bytes | `5ddf271e6fdf0babc58c2add6e4f9f7c7f26cc711cf7c669c664ef852f798b57` |
| `ps3scale.self` | 172,896 bytes | `f0e015884dd6ed6bcf5cf2a1fda4e343aa3ba393f69a6466f9924db9fc988414` |
| `ps3scale.pkg` | 181,648 bytes | `3748e2afc02a3812ba6e438259e2aa3a8926780d89acbac70c0ed809505eaa38` |
| `ps3scale.gnpdrm.pkg` | 181,648 bytes | `cd7810f4ad88bf3d7d086a1ec8203eb847988b1abc4516af09e71f680a8249fb` |

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
- MP3 asset is not embedded in the PKG yet; it must be copied once to the documented USRDIR path

## Console smoke test reported by user
A previous Alpha build launched successfully on the target CECH-2504A / CEX CFW 4.93 / Cobra 8.5 console. The user supplied photos showing:
- Main menu navigation screen
- PS3 game scan list with detected titles
- Game details page and local Minecraft ICON0 cover

Run 84 has passed CI but has not yet been installed on the console. The new vector icons and audio playback therefore still need a physical-console check.

## Remaining validation
1. Install the Run 84 package over the existing app (same package identity) and confirm the three vector icons render.
2. Copy the supplied MP3 to `/dev_hdd0/game/PS3SCALE1/USRDIR/theme.mp3`.
3. Confirm music starts automatically, continues across screens, loops, and responds to mute/volume.
4. Confirm audio shuts down cleanly when exiting the app.
5. Confirm Settings and About open and return correctly.
6. Test PS3/PS2 library switching and SELECT search using the controller.
7. Confirm local covers render correctly on the CECH-2504A.
8. Test PS2 CONFIG preview; confirm it does not write any game file.
9. Confirm no original game files or PARAM.SFO files changed.

## Not implemented yet
The actual verified graphics-profile workflow is still pending: online catalog/download, exact Title ID + update matching, backup/rollback, a CFW runtime plugin, the post-apply PS3 beep, and the in-game `Graphics applied — Enjoy!` notification. No unsupported game files are changed and no success notification is faked.
