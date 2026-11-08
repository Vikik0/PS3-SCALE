# mpg123 third-party notice

PS3 SCALE links the `libmpg123` MPEG audio decoder to decode the startup MP3 track.

- Upstream source: https://github.com/wargio/ps3soundlib/tree/262ea20d4e4e3fb5d0aca8a60d09d631c0e17fb8/libs/libmpg123
- Pinned upstream revision: `262ea20d4e4e3fb5d0aca8a60d09d631c0e17fb8`
- License: GNU Lesser General Public License, version 2.1 (LGPL-2.1)
- License text: `COPYING` in this directory.

The build workflow downloads this exact revision and builds only the `libmpg123` decoder. PS3 SCALE's audio output uses the PSL1GHT audio-port API. The GPL-licensed PS3 SoundLib audio-player and SPU-mixer components are not linked.
