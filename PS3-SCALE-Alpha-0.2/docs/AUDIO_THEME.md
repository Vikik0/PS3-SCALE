# Startup theme audio asset

## User-supplied file

- Name: `GTA V - Greyhound Pause Menu Theme Remix.mp3`
- Format: MP3 (MPEG Layer III)
- Sample rate: 44,100 Hz
- Channels: Stereo
- Duration: 7 minutes, 11.821 seconds
- File size: 6,663,260 bytes
- SHA-256: `468a33c6e85ee6b4a8966f69aeeaf82eb4a8d7d6d55bdc7891a9d5ceb958b896`

## Intended behavior

- Start the supplied track automatically when PS3 SCALE opens.
- Continue playing while the user moves between the app screens.
- Loop continuously.
- Settings must provide Music ON/OFF and volume control.
- Stop the audio thread and close the audio port cleanly when the app exits.

## Current implementation

The application now has an MP3 playback path based on the LGPL mpg123 decoder and the native PSL1GHT audio-port API. The build workflow fetches a pinned mpg123 source revision, builds the decoder, and links it into PS3 SCALE. Audio is streamed in 256-sample stereo blocks; the whole 7-minute track is not decoded into RAM at once.

The app checks these locations in order:

1. `/dev_hdd0/game/PS3SCALE1/USRDIR/theme.mp3`
2. `/dev_hdd0/game/PS3SCALE1/USRDIR/GTA V - Greyhound Pause Menu Theme Remix.mp3`
3. `/dev_usb000/PS3SCALE/theme.mp3` through `/dev_usb007/PS3SCALE/theme.mp3`

**One-time asset placement is still required:** the MP3 is not yet embedded in the PKG. Copy the supplied file to the first path and name it `theme.mp3` (or place it at one of the fallback paths). After that, startup playback is automatic and works offline. If the file is missing, the app continues to run without music and reports the missing track in Settings.

## Verification status

- [x] Native audio output and streaming MP3 decoder wired into the application.
- [x] Automatic playback request at app startup.
- [x] Looping, mute toggle and volume control implemented.
- [ ] GitHub Actions PS3 build passes with the new decoder dependency.
- [ ] Test playback, loop continuity, volume and shutdown on the target PS3.
- [ ] Bundle the MP3 in the install package so no separate file copy is needed.
