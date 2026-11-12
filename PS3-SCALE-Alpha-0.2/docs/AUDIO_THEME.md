# PS3 SCALE background music

## Music 2 — embedded in the PKG
- Track: GTA V - Greyhound Pause Menu Theme Remix
- Format: MP3, stereo, 44.1 kHz, 96 kbps CBR
- Size: 5,182,732 bytes
- Duration: approximately 7 minutes, 11.856 seconds
- SHA-256: `b8a7b1c2573ea8a397c2fbc3a6c731b25dcb71f572aedf5af8568785620d1b64`
- Installed path: `/dev_hdd0/game/PS3SCALE1/USRDIR/theme.mp3`

## Music 1 — Zorrovian BIOS
- Intended installed path: `/dev_hdd0/game/PS3SCALE1/USRDIR/music1.mp3`
- The MP3 is not yet part of the GitHub build inputs, so it is not embedded in this PKG.
- The app searches USB000–USB003 for `music1.mp3`, or the supplied filename `BIOS - Zorrovian [Official Visualiser] (1).mp3` / `BIOS - Zorrovian [Official Visualiser] (1)(2).mp3`, at the USB root or inside `PS3SCALE/`.
- It copies the file to a temporary internal-HDD path, validates it with mpg123, then atomically renames it to `USRDIR/music1.mp3`. The USB can be removed after import.
- If Music 1 is unavailable, the app reports that condition and plays the embedded Music 2 fallback.
- The build automatically packages Music 1 if `audio-source/music1.mp3` or ordered `audio-source/music1.mp3.part-*` inputs are added.

## Playlist behavior
- Auto alternates the first track on each app launch: one launch begins with Music 1 then Music 2; the next begins with Music 2 then Music 1.
- Music 1 only and Music 2 only are selectable in Settings.
- A one-second fade-in is applied at startup and when switching tracks.
- Music ON/OFF and volume remain available. Music OFF shuts down the decoder, audio port and worker thread.

## Implementation and verification
PS3 SCALE uses the LGPL mpg123 decoder and the native PSL1GHT audio-port API. MP3 data is decoded to 48 kHz stereo PCM blocks; the full track is not loaded into RAM. CI verifies the embedded Music 2 file and its SHA-256 inside the finalized GNPDRM PKG. Music 1 remains USB-importable but is not claimed as embedded until its binary is included in the source inputs.
