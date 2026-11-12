# PS3 SCALE Alpha 0.22 — visible staircase and XMB redesign

Date: 2026-11-12

## Why the previous console build did not match the reference
The previous build contained a Tiny3D staircase routine, but the console screenshot showed that the 3D pass was not visible. The UI therefore displayed only the flat gradient, orbital rings and particles. The Home card descriptions also overflowed their card widths, and an older saved settings file could keep the playlist on a single track.

## Corrections in Alpha 0.22
- Replaced the invisible 3D draw pass with a perspective-projected spiral staircase rendered in the regular 2D UI pass. Its 24 wedge-shaped treads, shaded risers, central support and handrail move in a slow, periodic loop.
- Removed the depth-buffer clear dependency that hid the earlier staircase.
- Kept the staircase behind every screen, not only Home, while preserving the animation ON/OFF setting.
- Shortened Home card descriptions so they fit without running into neighboring cards.
- Rebuilt the external XMB PIC1 wallpaper with a perspective spiral staircase; ICON0 and PIC0 remain generated in the same lavender visual identity.
- Added a Settings control for track fade duration (500 / 1000 / 1500 / 2000 ms).
- Added settings version migration: older settings files are moved to the requested AUTO playlist (BIOS - Zorrovian, then GTA V; the first track alternates on the next app launch) rather than remaining pinned to BIOS ONLY.
- Preserved Music ON/OFF, volume, animation toggle, language, Help, About and in-app issue reporting.
- Updated the displayed and package version to 00.22.

## Music assets
- Music 2 (GTA V pause-menu theme) is embedded in the PKG.
- Music 1 (BIOS - Zorrovian) is still imported from USB to the internal HDD; it is not embedded in the repository build inputs. The runtime supports the supplied original filename and the normalized `music1.mp3` name.
- AUTO playlist and configurable fade are available once Music 1 has been imported.

## Validation
- Latest GitHub Actions host-side tests: PASS.
- PS3 ELF / SELF / standard PKG / GNPDRM PKG build: PASS.
- XMB asset generation and package content checks: PASS (ICON0 320x176, PIC1 1280x720, PIC0 310x176; Apollo Save logo and Music 2 embedded).
- Physical PS3 Slim / CFW 4.92 verification: required. CI cannot prove the actual RSX appearance or audio playback on the console.

## Existing safety boundaries
- Scanning remains read-only; original game files are not replaced.
- Community NCL/TXT entries are downloaded as inert data only (HTTPS, 2 MB maximum, text-only extensions, no overwrite). They are never executed or auto-applied.
- Exact Title ID + APP_VER matching remains required for graphics profiles.
- Multi-write profiles remain disabled for automatic application.
- No universal AI upscaler, frame generation or guaranteed 60 FPS is claimed.

## Console verification order
Install the Alpha 0.22 GNPDRM package over the previous build, then check: splash, visible moving staircase, Home card text fit, Scan Games, Details, Online DB, Settings (AUTO playlist and fade duration), Help, About, and the external XMB preview. Confirm both music tracks play in the selected order after Music 1 is imported.
