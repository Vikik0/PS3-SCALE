# Bundling the startup theme in the PS3 PKG

The application looks for the installed resource at:

`/dev_hdd0/game/PS3SCALE1/USRDIR/theme.mp3`

The package rule copies everything under `pkgfiles/` into the package root. The supported bundle location is therefore:

`pkgfiles/USRDIR/theme.mp3`

## Build pipeline

The complete 7:11.856 track is stored in the repository as ordered binary chunks under `audio-source/theme.mp3.part-*`. The GitHub Actions PS3 build joins those chunks into `pkgfiles/USRDIR/theme.mp3` before `make pkg`.

The bundled MP3 is stereo, 44.1 kHz, 96 kbps, and 5,182,732 bytes. The build verifies its SHA-256 before packaging.

## PKG verification

After `make pkg`, `scripts/verify_pkg_theme.py` decrypts the finalized PS3 PKG's file table and confirms that:

- The package contains `USRDIR/theme.mp3`.
- The embedded file size is exactly 5,182,732 bytes.
- Its SHA-256 matches the packaged track.

This prevents a successful build from being mistaken for a music-embedded build when the asset is absent.

## Runtime

After installation, the song is available at `/dev_hdd0/game/PS3SCALE1/USRDIR/theme.mp3`. No USB drive or manual copy is needed for the built-in startup theme. The app retains its existing USB/HDD fallback behavior.

Real playback, looping, volume adjustment, and clean shutdown still require final testing on PS3 hardware.
