# PS3 SCALE UI flow

## Main menu
Four destinations:
1. **Scan Games** — PS3/PS2 game library.
2. **Settings** — audio, home animation, USB update check, library rescan and reset.
3. **Companion Tools** — reference information for Apollo Save Tool, Artemis PS3 and webMAN MOD.
4. **About** — version scope and safety boundary.

The home background uses only a pair of Tiny3D vector rings, four small moving points and one line. The animation advances at a slow frame-based rate and can be disabled. It does not use video, large textures, shaders or a full-screen post-process.

## Scan Games
- Separate PS3 and PS2 views; L1 selects PS3 and R1 selects PS2.
- The complete library counts are shown separately from the filtered result count.
- The game list is sorted by title, with a fixed-width left column.
- The selected title's name, type and ID are shown in a separate right column.
- Local ICON0 cover art is drawn below the metadata, not behind it.
- SELECT opens the native PS3 on-screen keyboard. The OSK callback uses a separate sysutil slot, collects the entered text through `oskGetInputText`, and applies the search after the keyboard is unloaded.
- Square rescans the library; Circle returns to the main menu.
- Scanning remains read-only.

## Game details
The details page shows the title, platform/type, title ID or PS2 serial, source path, local cover and profile status. A PS2 CONFIG preview is clearly marked as preview-only. The page does not claim a patch was applied.

## Settings
Settings are stored at `/dev_hdd0/game/PS3SCALE1/USRDIR/settings.cfg`:
- Background music ON/OFF.
- Music volume, adjusted with Left/Right.
- Home animation ON/OFF.
- Check for a PS3 SCALE PKG in the documented USB update folders.
- Rescan the game library.
- Restore default settings.

The USB update check is offline and only looks for a package file with a PKG header. It does not download, verify a release signature, or install the package. The user must install a confirmed PS3 SCALE update through Package Manager.

The MP3 is not embedded in the PS3 firmware or the current PKG. The app reads `/dev_hdd0/game/PS3SCALE1/USRDIR/theme.mp3` first, then the original filename in USRDIR, then `PS3SCALE/theme.mp3` on USB000–USB007.

## Companion tools
- **Apollo Save Tool** is a separate save/trophy/license utility.
- **Artemis PS3** is a separate cheat-code application with its own plugin.
- **webMAN MOD** is a separate game-mounting and system-utility plugin.

PS3 SCALE does not load these apps' plugins or modify their data.

## Performance and safety
- The animated home screen uses a few vector primitives only; it can be disabled.
- Muting music outputs silence without continuing MP3 decoding, reducing CPU work.
- Game scanning is read-only.
- No graphics profile is marked applied unless a future exact-title workflow verifies and applies it.
