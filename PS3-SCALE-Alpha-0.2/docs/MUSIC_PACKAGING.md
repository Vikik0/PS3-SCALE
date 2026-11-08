# Bundling the startup theme in the PS3 PKG

The application already looks for the installed resource at:

`/dev_hdd0/game/PS3SCALE1/USRDIR/theme.mp3`

The PSL1GHT package rule copies everything under `pkgfiles/` into the package root. Therefore the supported bundle location is:

`pkgfiles/USRDIR/theme.mp3`

Once the supplied MP3 is present at that path before `make pkg`, it is included in the installable PKG and the existing audio initialization will find it automatically. No external USB copy is needed after installation.

Run 122 does not contain the audio asset: the binary MP3 is not committed in this repository yet. The workflow now reports this explicitly instead of implying that the track is bundled. The app retains its existing USB/HDD fallback behavior.
