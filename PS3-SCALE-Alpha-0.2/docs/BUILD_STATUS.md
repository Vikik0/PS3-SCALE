# PS3 SCALE Alpha 0.2 — Build status

Date: 2026-11-08

## Latest GitHub Actions build
- Workflow run: [#94 — successful](https://github.com/Vikik0/PS3-SCALE/actions/runs/42486121782)
- Source commit: `41ad080d5a9f97ddc118f894441437defb5987ca`
- Artifact: [PS3-SCALE-Alpha-0.2-build](https://github.com/Vikik0/PS3-SCALE/actions/runs/42486121782/artifacts/13620080173)
- Artifact ID: `13620080173`
- Artifact ZIP SHA-256: `84f9abcdc01505a54d0ccec52dca9c3cb9fa947f881bedbe63ab6d3f97564367`

## Build outputs
- `ps3scale.elf`, `ps3scale.self`, `ps3scale.pkg`, and `ps3scale.gnpdrm.pkg` were generated.
- The finalized NPDRM PKG passed structural checks: PKG magic `7f504b47`, revision `8000`.
- Finalized PKG size: 181,712 bytes.
- Finalized PKG SHA-256: `89fb101bae38c05d590a064189da0e6114f12bec18cc46f47c9a6f298148e524`.

## Tests and current features
- Host-side SFO parser tests: PASS.
- Host-side scanner tests, including local cover discovery: PASS.
- Host-side ISO metadata tests: PASS.
- Host-side PS2 CONFIG builder tests: PASS.
- PPU ELF / CEX SELF / debug PKG / finalized NPDRM PKG: built successfully.
- Main menu has Tiny3D vector icons: magnifier for Scan Games, gear for Settings, information symbol for About.
- Separate PS3 and PS2 scan views, title search, game details, and local ICON0.PNG discovery/rendering.
- PS2 CONFIG preview only; no sidecar file is written or applied.
- Streaming MP3 decoder (LGPL mpg123) linked with native PSL1GHT audio output.
- Startup music auto-start, looping, mute toggle and volume controls implemented.
- Music file is read from internal USRDIR or USB000–USB007. It is not embedded in the PKG.

## Console smoke test reported by user
A previous Alpha build launched successfully on the target CECH-2504A / CEX CFW 4.93 / Cobra 8.5 console. The user supplied photos showing the main menu, PS3 game scan list, and game details page with a local Minecraft ICON0 cover.

Run #94 passed CI but has not yet been confirmed on the physical console. The vector icons and audio playback therefore still need a device-side check.

## Startup music setup
A companion USB bundle was prepared for the user. It contains the finalized PKG at the USB root and the supplied MP3 at `PS3SCALE/theme.mp3`. Extract both to the USB root, install the PKG, and leave the USB connected when opening PS3 SCALE. The app checks USB000 through USB007. For USB-free playback, copy the track to `/dev_hdd0/game/PS3SCALE1/USRDIR/theme.mp3`.

## Remaining validation
1. Install Run #94 over the existing app (same package identity) and confirm all three menu icons render.
2. Keep the music USB connected and confirm playback starts automatically on app launch.
3. Confirm music continues between screens, loops, and responds to mute/volume.
4. Confirm audio shuts down cleanly when exiting the app.
5. Confirm Settings/About navigation and return controls.
6. Test PS3/PS2 library switching, SELECT search, and local cover rendering.
7. Confirm PS2 CONFIG preview does not write any game file.
8. Confirm no original game files or PARAM.SFO files changed.

## Not implemented yet
The verified graphics-profile workflow is still pending: online catalog/download, exact Title ID + update matching, backup/rollback, a CFW runtime plugin, the post-apply PS3 beep, and the in-game `Graphics applied — Enjoy!` notification. No unsupported game files are changed and no success notification is faked.
