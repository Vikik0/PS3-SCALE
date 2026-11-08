# PS3 SCALE UI flow

## Main menu

Four destinations:
1. **Scan Games** — PS3/PS2 library.
2. **Settings** — audio, animation, USB package discovery, rescan, reset and music import.
3. **Companion Tools** — local detection for Apollo Save Tool, Artemis PS3 and webMAN MOD.
4. **About** — current scope and safety boundary.

The background uses two Tiny3D vector rings, four small moving points and one line. Motion is slow and low-complexity; it can be disabled in Settings.

## Scan Games

- L1 selects PS3; R1 selects PS2.
- Library totals and filtered result count are shown.
- The left panel contains the sorted game list.
- The right panel contains title, type, ID/serial and path.
- Cover art is isolated in the lower-right area so text does not draw over it.
- SELECT or START opens the native PS3 OSK search panel.
- The OSK callback collects text through \`oskGetInputText\`; the full-width alphabet panel is included in the allowed panel mask.
- Square rescans; Cross opens details; Circle returns home.
- Scanning remains read-only.

## Game details

The details page shows title, platform/type, title ID or PS2 serial, source path, cover and profile status. A PS2 CONFIG preview is clearly marked preview-only. It does not claim a patch was applied.

## Settings

Settings are stored at \`/dev_hdd0/game/PS3SCALE1/USRDIR/settings.cfg\`:
- Background music ON/OFF.
- Music volume with Left/Right.
- Home animation ON/OFF.
- Check for a PS3 SCALE PKG in the documented USB update folders.
- Rescan the game library.
- Restore default settings.
- Copy the music file from USB to the internal app USRDIR.

After music copy, the audio backend restarts and reopens the internal file. If the file was already present, the app also restarts the backend to prefer the internal path.

The USB update check is offline. It checks for a PKG header in a PS3 SCALE-specific folder; it does not download, verify a signed release manifest or install the package.

## Companion Tools

- **Apollo Save Tool:** save data, trophies and license management.
- **Artemis PS3:** title-specific cheat database and its own runtime plugin.
- **webMAN MOD:** game mounting, FTP, temperature and optional PS3MAPI services.

The page checks known local paths and displays INSTALLED / PLUGIN FOUND / NOT DETECTED. A different installation path can cause a false negative. CROSS refreshes the checks. PS3 SCALE does not load these applications' plugins or modify their files.

## Performance and safety

- Home animation uses a few vector primitives; it can be disabled.
- Muting music outputs silence without continuing MP3 decoding.
- Game scanning is read-only.
- No graphics profile is marked applied unless a future exact-title workflow verifies and applies it.
