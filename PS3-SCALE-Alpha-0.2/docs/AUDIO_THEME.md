# Startup theme audio asset

## User-supplied file

- Name: `GTA V - Greyhound Pause Menu Theme Remix.mp3`
- Format: MP3 (MPEG Layer III), stereo, 44.1 kHz
- Duration: approximately 7 minutes, 11.821 seconds
- File size: 6,663,260 bytes
- SHA-256: `468a33c6e85ee6b4a8966f69aeeaf82eb4a8d7d6d55bdc7891a9d5ceb958b896`

## Intended behavior

- Start the supplied track automatically when PS3 SCALE opens.
- Continue playing while the user moves between the app screens.
- Loop continuously.
- Settings provides Music ON/OFF and volume control.
- Stop the audio thread and close the audio port cleanly when the app exits.

## Current implementation

PS3 SCALE now links the LGPL mpg123 MP3 decoder and uses the native PSL1GHT audio-port API. It streams 48 kHz stereo PCM in 256-sample blocks, so the full track is not loaded into RAM. The startup path is initialized automatically before the game-library scan.

The app checks these locations in order:

1. `/dev_hdd0/game/PS3SCALE1/USRDIR/theme.mp3`
2. `/dev_hdd0/game/PS3SCALE1/USRDIR/GTA V - Greyhound Pause Menu Theme Remix.mp3`
3. `/dev_usb000/PS3SCALE/theme.mp3`
4. The same `PS3SCALE/theme.mp3` path on USB devices `/dev_usb001` through `/dev_usb007`.

A USB-ready bundle was prepared with:
- `ps3scale.gnpdrm.pkg` at the USB root.
- `PS3SCALE/theme.mp3` at the USB root folder.
- `README_AR.txt` with install and use instructions.

Extract the bundle to the USB root, install the PKG, and leave the USB connected when launching PS3 SCALE. The music should start automatically. For playback without USB, copy `theme.mp3` to the internal USRDIR path above using a PS3 file manager.

## Settings controls

- UP/DOWN: select Music or Volume.
- CROSS on Music: toggle ON/OFF.
- LEFT/RIGHT on Volume: decrease/increase volume.

## Verification status

- [x] Native audio output and streaming MP3 decoder linked.
- [x] Automatic playback initialization at app startup.
- [x] Continuous looping, mute toggle and volume control implemented.
- [x] Search internal HDD and USB000–USB007 paths.
- [x] GitHub Actions Run #94 built the ELF, SELF and finalized PKG successfully.
- [ ] Verify actual playback, loop continuity, volume and shutdown on the target CECH-2504A console.
- [ ] If the user wants a single self-contained PKG, add the MP3 as a package asset in a future build pipeline. Current USB bundle keeps the MP3 beside the PKG rather than inside it.
