# PS3 SCALE UI flow

## First launch and language
- If no valid language is stored in \`USRDIR/settings.cfg\`, show the language chooser before entering the app.
- Supported UI languages: English, Arabic and Spanish.
- Save the selection and allow changing it later from Settings.
- Arabic menu strings use the embedded bitmap glyph atlas with contextual forms and right-to-left run ordering. This is a focused UI implementation, not a full Unicode shaping/bidi engine; unsupported characters in user game titles may fall back to '?'.

## Startup / loading
- Load the packaged Apollo Save mark supplied by the user from the app's own \`USRDIR\`.
- Select the startup track from the saved Music Playlist mode.
- In automatic mode, Music 1 and Music 2 alternate between launches; track completion fades into the other track.
- Show a staged loading screen, check installed app files, refresh companion-tool status, and scan the local library on a worker thread.
- The progress bar represents startup stages, not a precise percentage of files scanned.
- No network download is needed for startup.

## Main menu
Five primary controller-focusable cards in one horizontal row:
1. **Scan Games** — PS3/PS2 game library.
2. **Online DB** — community graphics profiles and compatibility files.
3. **Settings** — music, playlist, volume, animation, language and maintenance.
4. **Help** — usage guide, issue form and feedback form.
5. **About** — version scope, hardware information and safety boundaries.

**Companion Tools** remains available from the Home screen with SELECT and lists Apollo Save Tool, Artemis PS3 and webMAN MOD. This keeps the five-card reference layout without removing the existing tools screen.

## Shared visual environment
- The in-app world uses a white / pale-lavender palette, glass-like cards, subtle purple selection glow and a large floating spiral staircase.
- A 22-tread Tiny3D staircase is rendered behind every page, not just Home.
- All animation is derived from a modulo phase and sine/cosine functions. The six-minute cycle returns to its starting pose and lighting; there is no accumulating translation or visible end-of-video cut.
- Daylight is the initial state. The background slowly transitions through dusk and a restrained night palette while content cards remain readable.
- The Home Animation setting disables the animated scene.

## Scan Games
- Separate PS3 and PS2 views; L1 selects PS3 and R1 selects PS2.
- The complete library counts are shown separately from the filtered result count.
- A left navigation rail, search strip, compact cover grid and selected-game summary share the page.
- The selected title's name, type and ID are shown in a separate detail card.
- Local ICON0 cover art is used when available; missing covers use a neutral lavender placeholder.
- SELECT or START opens the **PS3 SCALE custom controller keyboard**; the native system OSK is not invoked.
- The keyboard supports English/Spanish Latin keys, an Arabic layout, digits, space, backspace, clear, apply and cancel.
- Search text is UTF-8. CIRCLE cancels and restores the previous query.
- Square rescans the library; Circle returns to the main menu.
- Scanning remains read-only.

## Game details
- The page separates the game cover, platform/version/ID information, profile status and a visual comparison panel.
- The Original / Enhanced panel is a layout preview, not a claim that a real in-game screenshot has been captured.
- A profile is shown as available only when the existing exact-title checks pass.
- The page does not claim a patch was applied before the existing confirmation and runtime workflow completes.

## Settings
Settings are stored at \`/dev_hdd0/game/PS3SCALE1/USRDIR/settings.cfg\`:
- Background music ON/OFF.
- Music volume, adjusted with Left/Right.
- Home Animation ON/OFF.
- Language: English, Arabic or Spanish.
- Check for a PS3 SCALE PKG in the documented USB update folders.
- Rescan the game library.
- Restore default audio/animation and playlist settings.
- **Music Playlist:** Auto (alternate Music 1 / Music 2), Music 1 only, or Music 2 only.

Music 1 is the requested Zorrovian BIOS track. Music 2 is the existing bundled GTA V pause-menu theme at \`USRDIR/theme.mp3\`. Music 1 is not yet embedded in the current repository build: the runtime can import the exact supplied filename from USB000–USB003, but the source MP3 must be present in the build tree before it can ship inside PKG. Do not report Music 1 as bundled until the packaged file is verified.

The USB update check is offline and only looks for a package file with a PKG header. It does not download, verify a release signature, or install the package. The user must install a confirmed PS3 SCALE update through Package Manager.

## Online DB
- Search, filter and sync controls remain connected to the existing catalogue logic.
- The list and details panel use the same glass-card visual system.
- Community NCL files remain reference-only and are not installed or executed automatically.
- Exact Title ID / APP_VER checks remain required for applicable profiles.

## Help & Support
- The Help page contains five step cards and separate Issue / Feedback entry points.
- The report form uses the same controller keyboard as game search.
- Reports include the title, details, detected model, firmware, platform and app version.
- Every report is appended to \`USRDIR/support/outbox.txt\` before any network request, so a local copy survives a failed connection.
- If the separate support URL is configured, the app sends the report asynchronously over verified HTTPS. If the URL is still the build placeholder or the request fails, the local copy remains available.

## XMB identity
- The PKG build regenerates \`ICON0.PNG\`, \`PIC1.PNG\` and \`PIC0.PNG\` in the same bright staircase / lavender identity.
- The splash screen uses the new green Apollo Save logo asset supplied by the user.
- The in-app moving staircase is a real-time Tiny3D scene; XMB artwork remains static because XMB background support is handled separately.

## Performance and safety
- The loading scan runs separately from the UI thread so the progress screen can remain responsive.
- Muting music outputs silence without continuing MP3 decoding, reducing CPU work.
- Game scanning is read-only.
- No graphics profile is marked applied unless an exact-title workflow verifies and applies it.
