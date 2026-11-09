# PS3 SCALE UI flow

## First launch and language
- If no valid language is stored in `USRDIR/settings.cfg`, show the language chooser before entering the app.
- Supported UI languages: English, Arabic and Spanish.
- Save the selection and allow changing it later from Settings.
- Arabic menu strings use the embedded bitmap glyph atlas with contextual forms and right-to-left run ordering. This is a focused UI implementation, not a full Unicode shaping/bidi engine; unsupported characters in user game titles may fall back to '?'.

## Startup / loading
- Load the bundled Apollo Save logo from the app's own `USRDIR`.
- Select the startup track from the saved Music Playlist mode.
- In automatic mode, the first launch starts Music 1, then the next app launch starts Music 2; the next launch alternates back to Music 1.
- When a track finishes, automatic mode opens the other track and fades it in over one second.
- Show a staged loading screen, check installed app files, refresh companion-tool status, and scan the local library on a worker thread.
- The progress bar represents startup stages, not a precise percentage of files scanned.
- No network download is needed for startup.

## Main menu
Six controller-focusable tiles:
1. **Scan Games** — PS3/PS2 game library.
2. **Online DB** — community graphics profiles and compatibility files.
3. **Settings** — music, playlist, volume, animation, language and maintenance.
4. **Help & Support** — usage guide, issue form and feedback form.
5. **Companion Tools** — Apollo Save Tool, Artemis PS3 and webMAN MOD.
6. **About** — version scope, hardware information and safety boundaries.

The home background retains the low-poly 3D staircase / orbit ambience. It uses Tiny3D projection and matrix transforms and can be disabled in Settings.

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
- Restore default audio/animation and playlist settings.
- **Music Playlist:** Auto (alternate Music 1 / Music 2), Music 1 only, or Music 2 only.

Music 1 is the requested Zorrovian BIOS track. Music 2 is the existing bundled GTA V pause-menu theme at `USRDIR/theme.mp3`. The runtime looks for Music 1 at `USRDIR/music1.mp3`; the build helper packages it when its source is added under `audio-source/music1.mp3` or as ordered `music1.mp3.part-*` chunks. The current source branch still has only Music 2, so it reports the missing Music 1 asset and falls back to Music 2 until that file is added.

The USB update check is offline and only looks for a package file with a PKG header. It does not download, verify a release signature, or install the package. The user must install a confirmed PS3 SCALE update through Package Manager.

## Help & Support
- The Help page contains the five-step usage guide and separate Issue / Feedback entry points.
- The report form uses the same controller keyboard as game search.
- Reports include the title, details, detected model, firmware, platform and app version.
- Every report is appended to `USRDIR/support/outbox.txt` before any network request, so a local copy survives a failed connection.
- If the separate support URL is configured, the app sends the report asynchronously over verified HTTPS. If the URL is still the build placeholder or the request fails, the local copy remains available.
- The repository now contains the isolated Supabase Edge Function and migration. Online sending is not active in this build until a new dedicated Supabase project is deployed and its URL is placed in `PS3SCALE_SUPPORT_URL`.

## Companion tools
- **Apollo Save Tool** is a separate save/trophy/license utility. Its supplied green logo is packaged as an app asset and shown on the splash screen.
- **Artemis PS3** is a separate cheat-code application with its own plugin.
- **webMAN MOD** is a separate game-mounting and system-utility plugin.

PS3 SCALE does not load these apps' plugins or modify their data.

## Performance and safety
- The loading scan runs separately from the UI thread so the progress screen can remain responsive.
- Muting music outputs silence without continuing MP3 decoding, reducing CPU work.
- Game scanning is read-only.
- No graphics profile is marked applied unless an exact-title workflow verifies and applies it.
