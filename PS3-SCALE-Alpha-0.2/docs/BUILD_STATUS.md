# PS3 SCALE Alpha 0.21 — UI correction build

Date: 2026-11-12

## Corrections made after the first console screenshot
- Enabled RSX alpha testing and source-alpha blending for bitmap text, PNGs, translucent cards and glow layers. This targets the black rectangular text backgrounds visible in the earlier console screenshot.
- Cleared only the depth buffer between the 2D gradient and 3D scene so the background no longer occludes the staircase.
- Corrected per-vertex color submission for stair faces, orbital rings and satellite lines.
- Rebuilt the in-app staircase as a 32-tread Tiny3D spiral with a fixed periodic phase.
- Reworked the Home screen around the PS3 SCALE logo, five larger glass cards, CFW / console badges, library counts and controller hints.
- Reworked the external XMB ICON0, PIC1 wallpaper and PIC0 preview in the white/lavender staircase style.
- Added an XMB icon preview to the About screen.
- Updated the package version to 00.21.
- Music 2 (GTA V pause-menu theme) remains embedded and SHA-256 checked. Music 1 (BIOS - Zorrovian) is recognized under the exact uploaded filename and can be imported from USB; it is not embedded in this build because the MP3 is not present in the GitHub source inputs.

## Validation status
- Source changes are committed to `feature/online-graphics-database`.
- Host tests and PS3 ELF / SELF / PKG / GNPDRM build: pending the latest GitHub Actions run.
- XMB asset dimensions and package contents: pending CI verification.
- Physical PS3 Slim / CFW 4.92 visual verification: still required. CI cannot prove the exact RSX appearance on the console.

## Existing safety boundaries
- Scanning is read-only; original game files are not replaced.
- Community NCL index entries remain searchable references only; arbitrary community files are not executed.
- Exact Title ID + APP_VER matching remains required for graphics profiles.
- Multi-write profiles remain disabled for automatic application.
- No universal AI upscaler, frame generation or guaranteed 60 FPS is claimed.

## Next console check
Install the new GNPDRM package over the previous Alpha build and verify, in order: splash, moving staircase, clean text without black boxes, Home cards, Scan Games, Details, Online DB, Settings, Help and About. Confirm Music 2 plays; to use Music 1, copy the supplied MP3 to a USB drive root with its original filename or rename it to `music1.mp3`. Then test one game launch and report any freeze or rendering artifact.
