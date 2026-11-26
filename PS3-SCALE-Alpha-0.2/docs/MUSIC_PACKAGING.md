# PS3 SCALE two-track music packaging

## Music 2 — built into the PKG
- GTA V - Greyhound Pause Menu Theme Remix
- Installed as `USRDIR/theme.mp3`.
- Reconstructed from the 14 ordered `audio-source/theme.mp3.part-*` binary chunks.
- SHA-256: `b8a7b1c2573ea8a397c2fbc3a6c731b25dcb71f572aedf5af8568785620d1b64`.

## Music 1 — Zorrovian BIOS
- Installed as `USRDIR/music1.mp3`.
- The app auto-imports the uploaded Zorrovian MP3 from USB000–USB003, at the root or in `PS3SCALE/`.
- Accepted names: `music1.mp3`, `BIOS - Zorrovian [Official Visualiser] (1).mp3`, and `BIOS - Zorrovian [Official Visualiser] (1)(2).mp3`.
- Import uses a temporary file, mpg123 validation and atomic rename. Once copied, the internal HDD copy is used on later launches.
- The original uploaded MP3 is committed at `audio-source/music1.mp3` (3,129,868 bytes; SHA-256 `8b567be77af069f8c0010d54f6046b4ae339bf4599d2667939aa45d1d746cf23`).
- `make pkg` installs it as `pkgfiles/USRDIR/music1.mp3`, so the finalized GNPDRM PKG contains the track without requiring a USB import.
- CI requires the source, checks its exact size and SHA-256, and runs the finalized PKG verifier with `--require-music1`; a missing or incorrect BIOS source fails the build.

## Playback
- Auto alternates the first track on each app launch.
- Music 1 only and Music 2 only are selectable.
- Fade-in is one second; music volume and ON/OFF remain configurable.
