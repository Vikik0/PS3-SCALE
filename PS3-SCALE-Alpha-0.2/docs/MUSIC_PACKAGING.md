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
- The uploaded MP3 is not yet stored in GitHub build inputs; this build therefore does not embed Music 1. Adding `audio-source/music1.mp3` or ordered `music1.mp3.part-*` chunks makes the existing packaging script include it.

## Playback
- Auto alternates the first track on each app launch.
- Music 1 only and Music 2 only are selectable.
- Fade-in is one second; music volume and ON/OFF remain configurable.
