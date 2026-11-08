# Startup theme audio asset

## Supplied track and packaged copy

- Track: GTA V - Greyhound Pause Menu Theme Remix
- Source format: MP3, stereo, 44.1 kHz, 6,663,260 bytes
- Packaged format: MP3, stereo, 44.1 kHz, 96 kbps CBR
- Packaged size: 5,182,732 bytes
- Duration: approximately 7 minutes, 11.856 seconds
- Packaged SHA-256: `b8a7b1c2573ea8a397c2fbc3a6c731b25dcb71f572aedf5af8568785620d1b64`

The full track is retained. It is re-encoded at 96 kbps stereo to reduce the installer size while keeping the complete music.

## Installed location

The theme is included inside the PS3 SCALE PKG at:

`USRDIR/theme.mp3`

After installation it is available at:

`/dev_hdd0/game/PS3SCALE1/USRDIR/theme.mp3`

The app can therefore start the theme without a USB drive or a manual copy step.

## Intended behavior

- Start automatically when PS3 SCALE opens if Music is enabled.
- Continue while moving between app screens.
- Loop continuously.
- Settings exposes Music ON/OFF and volume.
- Stop the audio thread and close the audio port cleanly when the app exits.

## Implementation

PS3 SCALE links the LGPL mpg123 decoder and uses the native PSL1GHT audio-port API. MP3 is streamed in 48 kHz stereo PCM blocks; the complete track is not loaded into RAM.

The app retains USB fallback paths for recovery or alternate user-provided themes, but the standard installer now carries the startup track internally.

## Build and verification

The repository stores the MP3 as ordered binary source chunks under `audio-source/`. The PS3 build workflow joins them into `pkgfiles/USRDIR/theme.mp3` before packaging.

The workflow then decrypts the finalized PKG data table and verifies that the exact `USRDIR/theme.mp3` entry and SHA-256 are present in the installable package.

- [x] Full track included in the PKG build input.
- [x] 96 kbps stereo packaged copy prepared.
- [x] Build-time PKG-content verification added.
- [ ] Verify real playback, loop continuity, volume, and clean shutdown on the user's PS3 hardware.
