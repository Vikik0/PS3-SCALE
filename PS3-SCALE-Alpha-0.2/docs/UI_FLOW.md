# PS3 SCALE UI flow

## First launch and language
- If no valid language is stored in `USRDIR/settings.cfg`, show a language chooser before entering the app.
- Supported UI languages: English, Arabic and Spanish.
- Save the selection and allow changing it later from Settings.
- Arabic menu strings use a compact embedded bitmap glyph atlas with contextual Arabic forms and right-to-left run ordering. This is a focused UI implementation, not a full Unicode shaping/bidi engine; unsupported characters in user game titles may fall back to '?'.

## Startup / loading
- Start the selected startup music after graphics and preferences are initialized.
- Show a short staged loading screen.
- Check that the installed app files can be found.
- Refresh the companion-tool presence status.
- Scan the local game library on a worker thread while the loading screen animates.
- The progress bar shows stages, not a precise percentage of files scanned.
- No network download is needed for startup and no arbitrary files are downloaded automatically.

## Main menu
Four destinations:
1. **Scan Games** — PS3/PS2 game library.
2. **Settings** — audio, home animation, language, USB update check, library rescan and reset.
3. **Companion Tools** — reference information for Apollo Save Tool, Artemis PS3 and webMAN MOD.
4. **About** — version scope and safety boundary.

The home background uses a slowly rotating low-poly 3D wireframe core with two orbit rings. It uses Tiny3D projection/matrix transforms and a small number of line vertices; it can be disabled in Settings.

## Scan Games
- Separate PS3 and PS2 views; L1 selects PS3 and R1 selects PS2.
- The complete library counts are shown separately from the filtered result count.
- The game list is sorted by title, with a fixed-width left column.
- The selected title's name, type and ID are shown in a separate right column.
- Local ICON0 cover art is drawn below the metadata, not behind it.
- SELECT or START opens the **PS3 SCALE custom controller keyboard**; the native system OSK is not invoked.
- The keyboard supports English/Spanish Latin keys, an Arabic layout, digits, space, backspace, clear, apply and cancel.
- Search text is UTF-8. CIRCLE cancels and restores the previous query.
- Square rescans the library; Circle returns to the main menu.
- Scanning remains read-only.

## Game details
The details page shows the title, platform/type, title ID or PS2 serial, source path, local cover and profile status. A PS2 CONFIG preview is clearly marked as preview-only. The page does not claim a patch was applied.

## Settings
Settings are stored at `/dev_hdd0/game/PS3SCALE1/USRDIR/settings.cfg`:
- Background music ON/OFF.
- Music volume, adjusted with Left/Right.
- Home animation ON/OFF.
- Language: English, Arabic or Spanish.
- Check for a PS3 SCALE PKG in the documented USB update folders.
- Rescan the game library.
- Restore default audio/animation settings.
- Copy the music MP3 from USB to internal USRDIR.

The USB update check is offline and only looks for a package file with a PKG header. It does not download, verify a release signature, or install the package. The user must install a confirmed PS3 SCALE update through Package Manager.

The MP3 is not embedded in the PS3 firmware or current PKG. The app reads internal USRDIR first, then the original filename in USRDIR, then `PS3SCALE/theme.mp3` on USB000–USB007.

## Companion tools
- **Apollo Save Tool** is a separate save/trophy/license utility.
- **Artemis PS3** is a separate cheat-code application with its own plugin.
- **webMAN MOD** is a separate game-mounting and system-utility plugin.

PS3 SCALE does not load these apps' plugins or modify their data.

## Performance and safety
- The home animation uses a few low-poly wireframe primitives only; it can be disabled.
- The loading scan runs separately from the UI thread so the progress screen can remain responsive.
- Muting music outputs silence without continuing MP3 decoding, reducing CPU work.
- Game scanning is read-only.
- No graphics profile is marked applied unless a future exact-title workflow verifies and applies it.
