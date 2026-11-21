# PS3 SCALE UI flow — Alpha 0.25

## First launch and language

- If no valid language is stored in `USRDIR/settings.cfg`, show the language chooser before entering the app.
- Supported UI languages: English, Arabic and Spanish.
- Save the selection and allow changing it later from Settings.
- Arabic menu strings use the embedded bitmap glyph atlas with contextual forms and right-to-left run ordering. This is a focused UI implementation, not a full Unicode shaping/bidi engine; unsupported characters in user game titles may fall back to '?'.

## Startup / loading

- Load the packaged Apollo Save mark supplied by the user from the app's own `USRDIR`.
- Select the startup track from the saved Music Playlist mode.
- In automatic mode, Music 1 and Music 2 alternate between launches; track completion fades into the other track.
- Show a staged loading screen, check installed app files, refresh companion-tool status, and scan the local library on a worker thread.
- The progress bar represents startup stages, not a precise percentage of files scanned.
- No network download is needed for startup. Online DB starts its first session refresh only when the user opens that page.

## Main menu

Five primary controller-focusable cards in one horizontal row:

1. **Scan Games** — PS3/PS2 game library.
2. **Online DB** — community graphics/FPS files and exact-version profile metadata.
3. **Settings** — music, playlist, volume, animation, language and maintenance.
4. **Help** — usage guide, issue form and feedback form.
5. **About** — version scope, hardware information and safety boundaries.

The Home page no longer displays a separate FEATURES heading. The About page uses the full information card and no longer repeats the XMB icon as an extra image.

**Companion Tools** remains available from the Home screen with SELECT and lists Apollo Save Tool, Artemis PS3 and webMAN MOD. This keeps the five-card reference layout without removing the existing tools screen.

## Shared visual environment

- The in-app world uses a pearl-white / pale-lavender palette, glass-like cards, subtle purple selection glow and one large floating spiral staircase.
- 32 visible treads scroll past a camera in a repeating 18-tread helix.
- All animation is derived from a modulo phase and sine/cosine functions. The 21,600-frame cycle is a continuous virtual ascent with a repeating camera pose.
- Daylight is the initial state. The background slowly transitions through dusk and a restrained night palette while content cards remain readable.
- The Home Animation setting disables the animated scene.

## Scan Games

- Separate PS3 and PS2 views; L1 selects PS3 and R1 selects PS2.
- The complete library counts are shown separately from the filtered result count.
- A left navigation rail, search strip, compact cover grid and selected-game summary share the page.
- The selected title's name, type and ID are shown in a separate detail card.
- Local ICON0 cover art is used when available; missing covers use a neutral lavender placeholder.
- SELECT or START opens the PS3 SCALE custom controller keyboard; the native system OSK is not invoked.
- The keyboard supports English/Spanish Latin keys, an Arabic layout, digits, space, backspace, clear, apply and cancel.
- Search text is UTF-8. CIRCLE cancels and restores the previous query.
- Square rescans the library; Circle returns to the main menu.
- Scanning remains read-only.

## Game Details and real previews

- The page separates the game cover, platform/version/ID information, profile status and a before/after comparison area.
- A comparison is drawn only when both local PNGs match the selected game's Title ID and APP_VER.
- The required filenames are `<TITLE_ID>_<APP_VER>_before.png` and `<TITLE_ID>_<APP_VER>_after.png` in `/dev_hdd0/game/PS3SCALE1/USRDIR/previews/`.
- If either image is missing, the UI shows NO MATCHED SCREENSHOTS. It does not draw a generic concept scene or use promotional cover art as a substitute.
- The repository does not currently bundle real before/after pairs for every game; genuine captures must be supplied. See `docs/GAME_PREVIEWS.md`.

## Settings

Settings are stored at `/dev_hdd0/game/PS3SCALE1/USRDIR/settings.cfg`:

- Background music ON/OFF.
- Music volume, adjusted with Left/Right.
- Home Animation ON/OFF.
- Language: English, Arabic or Spanish.
- Check for a PS3 SCALE PKG in the documented USB update folders.
- Rescan the game library.
- Restore default audio/animation and playlist settings.
- **Music Playlist:** Auto (alternate Music 1 / Music 2), Music 1 only, or Music 2 only.
- **Enhancement Storage:** Temporary (default) or Permanent for newly installed Artemis NCL files.

Music 1 is the requested Zorrovian BIOS track. Music 2 is the bundled GTA V pause-menu theme at `USRDIR/theme.mp3`. The Settings UI calls the entries Music 1 and Music 2; the source descriptions remain available in status/help text.

Temporary NCL files are removed the next time PS3 SCALE starts. Permanent files remain in Artemis USERLIST until the user removes them. Changing the setting does not retroactively change previously installed files.

The USB update check is offline and only looks for a package file with a PKG header. It does not download, verify a release signature, or install the package. The user must install a confirmed PS3 SCALE update through Package Manager.

## Online DB

- First open in an app session starts a real HTTPS refresh automatically while the last valid local cache remains available.
- Triangle forces a profile + community index refresh; Start remains an alternate refresh shortcut.
- Select opens search; Square cycles the result filter. The default is All Files + Any Region/Version. Every indexed community entry is visible on first open, including unmatched entries; unmatched entries are browse-only until the exact local game is scanned.
- Cross installs an eligible NCL file into the detected Artemis USERLIST directory. It is available only for a scanned HDD game with an exact Title ID + APP_VER match, a `GRAPHICS_FPS` category and an installed Artemis app.
- The app supports the upstream Artemis `ARTPS3001` package ID and the Artemis R5 `ARTZ00001` package ID.
- Downloads are limited to small plain-text NCL files. Existing files are not overwritten.
- PS3 SCALE does not execute or auto-apply an NCL file. The user opens Artemis and selects the matching code.
- A profile result without a community NCL opens Game Details instead of starting a download.

## FPS profiles and webMAN

- The FPS Control screen uses only existing exact Title ID + APP_VER memory profiles.
- CROSS or START in Game Details is explicit confirmation: an exact one-write B_COMMUNITY profile is armed automatically, then the game launches. If no exact profile exists, the original game launches unmodified.
- Triangle in Game Details and the separate FPS Control workflow can still arm a profile without launching immediately; webMAN MOD and PS3MAPI are required.
- Normal game launch requires webMAN's local HTTP endpoint, not PS3MAPI.
- If PS3MAPI is unavailable after a profile is armed, PS3 SCALE removes its own one-launch script and still attempts to launch the game unmodified.
- If webMAN itself is unavailable, the app displays a specific network/HTTP error and remains responsive.
- The one-launch webMAN script checks Title ID, in-game state and expected original bytes. It does not modify EBOOT or ISO files.

## Help & Support

- The Help page contains five step cards and separate Issue / Feedback entry points.
- The report form uses the same controller keyboard as game search.
- Reports include the title, details, detected model, firmware, platform and app version.
- Every report is appended to `USRDIR/support/outbox.txt` before any network request, so a local copy survives a failed connection.
- If the separate support URL is configured, the app sends the report asynchronously over verified HTTPS. If the URL is still the build placeholder or the request fails, the local copy remains available.

## XMB identity

- `ICON0.PNG` and the large selected-item preview `PIC0.PNG` are both generated from the same approved square Neon Glass logo. `PIC1.PNG` remains the full-screen single-staircase pearl/lavender wallpaper.
- The splash screen uses the new green Apollo Save logo asset supplied by the user.
- The in-app moving staircase is a real-time Tiny3D scene; XMB artwork remains static because XMB background support is handled separately.

## Performance and safety

- The loading scan runs separately from the UI thread so the progress screen can remain responsive.
- Muting music outputs silence without continuing MP3 decoding, reducing CPU work.
- Game scanning is read-only.
- No graphics profile is marked applied unless the exact-title workflow completes.
- Community NCL files are downloaded as separate Artemis USERLIST files; they are not copied over original game files and are not auto-applied.
