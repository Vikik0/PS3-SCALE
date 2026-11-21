# Per-game before/after screenshots

Game Details no longer draws a generic concept illustration. It loads a real screenshot pair only when both files exist for the selected PS3 Title ID and APP_VER.

## Runtime folder

Place the PNG files on the console in:

`/dev_hdd0/game/PS3SCALE1/USRDIR/previews/`

For a game with Title ID `BLUS30464` and APP_VER `01.05`, the exact filenames are:

- `BLUS30464_01.05_before.png`
- `BLUS30464_01.05_after.png`

The `before` image must show the original game build. The `after` image must show the same scene and game build with the referenced enhancement applied. Use the exact APP_VER read from the game's PARAM.SFO.

## Image requirements

- PNG format.
- 16:9 aspect ratio; 640×360 is recommended. 320×180 is also suitable.
- Keep both images framed to the same scene, camera position and crop.
- Do not use cover art, promotional artwork, generated concept art or unrelated screenshots as a before/after pair.
- Both files are required. If either is missing, the UI shows the “NO MATCHED SCREENSHOTS” state instead of inventing a comparison.

## Source and verification

The app matches filenames to Title ID + APP_VER. It does not independently authenticate the image source or prove that the after image came from a particular patch. Only add screenshots captured from the actual original and enhanced game builds, and retain the source/patch reference with the asset when distributing preview packs.

No per-game screenshot pairs are currently bundled in the repository. The loader and exact-version file convention are in place, but actual pairs must be supplied from genuine captures before the UI can show a comparison for a title.
