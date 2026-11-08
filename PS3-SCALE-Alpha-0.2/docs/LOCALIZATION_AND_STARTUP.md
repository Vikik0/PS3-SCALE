# Localization, first-run setup and startup flow

## First launch
- When no saved language is present, PS3 SCALE opens a language chooser before entering the app.
- Available interface languages are English, Arabic and Spanish.
- The choice is stored as `language=0|1|2` in `USRDIR/settings.cfg`.
- The Settings page includes a Language row; CROSS or LEFT/RIGHT cycles the three choices.
- Existing settings for music, volume and home animation are preserved.

## Text rendering
- The app uses three font ranges: a compact Latin ASCII bitmap font, a Noto Sans Arabic UI TTF rendered through FreeType, and a small bitmap range for Spanish accent extras.
- The pinned Noto Sans Arabic UI variable font is fetched at build time from Google Fonts, hash-verified, and bundled into the PKG as `USRDIR/NotoSansArabicUI.ttf`; its upstream OFL license is bundled beside it.
- Arabic UI strings are shaped into isolated/final/initial/medial presentation forms and reordered for a right-to-left display run before drawing.
- The Arabic TTF callback uses FreeType's glyph bitmap bearings and advance widths. The renderer therefore advances by the font's real glyph metrics instead of guessing overlap from bitmap bounds; connected letters join according to the font's contextual forms, while non-joining letters retain their natural spacing.
- English ASCII retains its original bitmap font and left-to-right order. Spaces in mixed Arabic/English strings use the correct width for the adjacent run.
- Font-size changes are tracked by the UI renderer so Arabic spacing remains proportional on the loading screen, menus, settings, scanner, details and controller keyboard.
- The embedded Arabic shaping map is intentionally small and tailored to the app UI. It is not a complete Unicode, HarfBuzz or ICU implementation. Unsupported characters in arbitrary game titles fall back to a question mark; vowel marks/diacritics and advanced mixed-direction edge cases are not promised.
- The TTF is rasterized into the RSX bitmap atlas once during initialization; the app does not rasterize text every frame.

## Search keyboard
- SELECT or START opens PS3 SCALE's own controller-driven keyboard.
- The keyboard has English/Spanish Latin layouts and an Arabic layout, plus digits, space, backspace, clear, apply and cancel.
- CROSS selects a key, the D-pad moves, and CIRCLE cancels without replacing the previous query.
- Search text is UTF-8; backspace removes one complete UTF-8 code point.
- The native PS3 system OSK is no longer used by the search screen.

## Startup screen
The startup sequence performs local work only:
1. Initialize the renderer and controller.
2. Load saved preferences and start the configured audio engine.
3. Check that the installed application files are present.
4. Check companion-tool presence.
5. Scan local PS3/PS2 library paths in a worker thread while the loading screen animates.
6. Enter the main menu.

The loading bar represents startup stages, not a precise byte-by-byte scan percentage. The library scanner remains read-only.

## Network and update policy
- Startup does not download arbitrary files. The app does not need a network download to open, render its UI, play the supplied music file or scan locally mounted game paths.
- The current update action searches supported USB folders for a PS3 SCALE PKG and checks its PKG magic. It does not validate a cryptographic release signature, fetch GitHub releases, or install a package automatically.
- A future online updater should require HTTPS certificate validation, a signed release manifest, SHA-256 verification, explicit user consent and a safe rollback path. Do not add unattended downloads or auto-installation.

## Animation and performance
- The home-screen emblem uses Tiny3D's 3D projection and matrix transforms with a low-poly wireframe core and two line-loop orbits.
- The animation advances slowly and can be disabled in Settings.
- It uses no full-screen video, shader post-processing or large per-frame texture uploads.
- The startup screen reuses the same small vector model.

## Verification boundary
Host tests can validate localization mapping, UTF-8 handling, keyboard layouts, the static UI contract, scanner behavior and the PS3 package build. The actual Arabic glyph appearance, controller navigation, 3D frame pacing, audio playback and boot scan duration still require testing on the target PS3 console.
