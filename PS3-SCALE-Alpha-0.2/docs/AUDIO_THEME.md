# Startup theme audio asset

## User-supplied file

- Name: \`GTA V - Greyhound Pause Menu Theme Remix.mp3\`
- Format: MP3, stereo, 44.1 kHz
- Duration: approximately 7 minutes, 11.821 seconds
- Size: 6,663,260 bytes
- SHA-256: \`468a33c6e85ee6b4a8966f69aeeaf82eb4a8d7d6d55bdc7891a9d5ceb958b896\`

## Intended behavior

- Start automatically when PS3 SCALE opens if the file is available.
- Continue while moving between app screens.
- Loop continuously.
- Settings exposes Music ON/OFF and volume.
- Stop the audio thread and close the audio port cleanly when the app exits.

## Implementation

PS3 SCALE links the LGPL mpg123 decoder and uses the native PSL1GHT audio-port API. MP3 is streamed in 48 kHz stereo PCM blocks; the complete track is not loaded into RAM.

The app checks:
1. \`/dev_hdd0/game/PS3SCALE1/USRDIR/theme.mp3\`
2. \`/dev_hdd0/game/PS3SCALE1/USRDIR/GTA V - Greyhound Pause Menu Theme Remix.mp3\`
3. \`/dev_usb000/PS3SCALE/theme.mp3\`
4. The same USB path on \`/dev_usb001\` through \`/dev_usb007\`.

## USB bundle and internal copy

The USB bundle contains:
- \`ps3scale.gnpdrm.pkg\` at the USB root.
- \`PS3SCALE/update/ps3scale.gnpdrm.pkg\` for the app's offline update-file discovery.
- \`PS3SCALE/theme.mp3\` as a separate audio asset.

The MP3 is **not embedded in the PKG**. The user can keep it on USB or copy it to the app's internal USRDIR from Settings → **COPY MUSIC TO INTERNAL HDD**. After copying, the app shuts down the current audio backend and initializes it again; it should then select the internal copy immediately without requiring an app restart.

## Controls

- CROSS on BACKGROUND MUSIC: toggle ON/OFF.
- LEFT/RIGHT on MUSIC VOLUME: adjust volume.
- HOME ANIMATION can be toggled separately.

## Verification

- [x] Streaming MP3 decoder linked.
- [x] Automatic startup initialization and looping implemented.
- [x] Music ON/OFF and volume controls implemented.
- [x] Internal HDD and USB000–USB007 search paths implemented.
- [x] Copy-to-HDD action and immediate audio-backend restart implemented.
- [x] GitHub Actions Run #115 built ELF, SELF and finalized NPDRM PKG.
- [ ] Verify real playback, loop continuity, volume, USB removal after copy, and clean shutdown on CECH-2504A.
