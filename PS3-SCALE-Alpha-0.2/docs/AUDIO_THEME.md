# PS3 SCALE startup music

## Music 2 — existing bundled theme
- Track: GTA V - Greyhound Pause Menu Theme Remix
- Packaged format: MP3, stereo, 44.1 kHz, 96 kbps CBR
- Packaged size: 5,182,732 bytes
- Duration: approximately 7 minutes, 11.856 seconds
- SHA-256: `b8a7b1c2573ea8a397c2fbc3a6c731b25dcb71f572aedf5af8568785620d1b64`
- Installed path: `/dev_hdd0/game/PS3SCALE1/USRDIR/theme.mp3`

## Music 1 — requested Zorrovian BIOS track
- Intended installed path: `/dev_hdd0/game/PS3SCALE1/USRDIR/music1.mp3`
- The Music 1 source file is identified and its expected size/SHA-256 are now enforced by the packaging scripts, but its binary chunks still need to be committed under `audio-source/` before it can ship inside the PKG.
- The app now searches `/dev_usb000` through `/dev_usb003` for `music1.mp3` or the exact supplied filename `BIOS - Zorrovian [Official Visualiser] (1).mp3`, at the USB root or in a `PS3SCALE/` folder. Before the audio thread starts, it copies a valid file to the internal HDD path above using a temporary file and atomic rename.
- Once imported, the app plays the internal HDD copy on every launch; the USB drive can be removed after the copy completes.
- If the file is neither embedded nor found on USB, the app reports the missing asset and falls back to Music 2.
- `scripts/prepare_theme.py` packages Music 1 automatically when `audio-source/music1.mp3` or ordered `audio-source/music1.mp3.part-*` files are present.

## Playlist behavior
- **Auto:** every app launch starts Music 2 (GTA V), then Music 1 (BIOS - Zorrovian) when Music 2 ends. The playlist then alternates Music 2 / Music 1 until the app closes.
- **Music 1 only:** loops Music 1.
- **Music 2 only:** loops Music 2.
- The Settings page exposes Auto, Music 1 only and Music 2 only.
- A one-second fade-in at the start of each track and a one-second fade-out at the end of each track are calculated from the decoder's sample position. Track changes happen on a block boundary so samples from two songs are never mixed accidentally.
- Music ON/OFF and volume remain available; Music OFF shuts down the decoder, audio port and worker thread.

## Implementation
PS3 SCALE uses the LGPL mpg123 decoder and the native PSL1GHT audio-port API. MP3 is decoded in 48 kHz stereo PCM blocks; the full track is not loaded into RAM. The track switch occurs in the audio worker, not the controller/UI loop.

## Build and verification
The build reconstructs Music 2 from ordered source chunks and verifies its exact SHA-256 inside the finalized PKG. The package verifier also checks that the Apollo Save logo is present with its expected size and SHA-256.

- [x] Existing Music 2 theme is embedded and verified.
- [x] Two-track playback and the fixed Music 2-first launch order are implemented.
- [x] Playlist selector and per-track fade-in/fade-out are implemented.
- [ ] Commit the exact Zorrovian BIOS MP3 binary chunks so it ships inside the PKG without a USB import.
- [ ] Verify both tracks, transitions, volume and clean shutdown on the user's PS3 hardware.
