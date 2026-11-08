# Startup theme audio asset

## User-supplied file

- Name: `GTA V - Greyhound Pause Menu Theme Remix.mp3`
- Format: MP3 (MPEG Layer III)
- Sample rate: 44,100 Hz
- Channels: Stereo
- Duration: 7 minutes, 11.821 seconds
- File size: 6,663,260 bytes

## Intended behavior

The user wants this music to play when PS3 SCALE starts. It should be possible to mute it in Settings and avoid restarting playback when navigating between screens. It must stop cleanly when the app exits or the user disables the theme.

## Implementation status

The MP3 has been received in the conversation, but has not yet been committed into this GitHub repository or packaged in the PKG. The current PS3 toolchain image installs PSL1GHT, Tiny3D, font3d and YA2D; an MP3 decoder/playback library is not currently declared in this project. Playback must not be marked complete until a supported PS3 decoder/audio path is integrated and tested on the target console.

The next audio work is to add the binary asset through a proper binary upload or release asset workflow, select a decoder supported by the PS3 toolchain, and test startup, looping, volume/mute and clean shutdown on hardware.
