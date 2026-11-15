# PS3 SCALE — Cinematic Spiral Staircase Background

## Visual design
The in-app background follows the approved pearl-white / pale-lavender PS3 SCALE reference:
- a floating spiral staircase with 32 broad, softly shaded treads,
- white and silver tread tops, pale-lilac risers, and restrained violet edge lighting,
- inner and outer spiral handrails with fine balusters,
- a quiet lavender gradient, subtle light ribbons, a restrained halo, and low-contrast particles,
- background contrast kept low enough for the glass UI cards and game covers to remain readable.

The Apollo Save logo remains a separate splash asset and is not recolored.

## Camera motion and seamless loop
- The staircase geometry remains fixed; the camera supplies the motion through perspective and parallax.
- The camera follows a slow forward dolly with a shallow lateral arc while continuously looking toward the staircase. It does not pause at the loop boundary.
- The path is periodic over 21,600 frames (six minutes at 60 FPS). Camera position, viewing direction, background light phase, and glow all return to their initial values together.
- The path is built from sine and cosine functions, so the camera travels continuously around a closed cinematic track instead of moving forward and then abruptly teleporting or reversing.
- Home Animation can be disabled in Settings.

## Rendering and performance
- The staircase is procedural 3D geometry with a perspective camera transform, projected in software into Tiny3D's stable 2D rendering pass. This deliberately avoids relying on RSX depth-buffer behavior that previously hid the scene.
- Faces are transformed into camera space and sorted back-to-front; faces and line segments crossing the near plane are skipped rather than stretched across the screen.
- Tread surfaces and outlines are submitted in two batched draws; inner and outer rails, balusters, and tread-edge highlights use batched line draws to reduce RSX submission overhead.
- RSX alpha testing and source-alpha blending remain enabled for transparent bitmap glyphs, PNG cover art, glass cards, and glow layers.
- The same scene is drawn behind Home, Scan, Details, Online DB, Settings, Tools, About, Help, Support, language selection, and keyboard screens.
- No heap allocations, texture creation, or file I/O occur in the frame loop.

## XMB artwork
- `ICON0.PNG` is the external PS3 SCALE application icon.
- `PIC1.PNG` is the bright white/lavender staircase wallpaper with quiet negative space for XMB icons.
- `PIC0.PNG` is the compact XMB preview tile.
- These are static package artwork; the in-app staircase is generated and animated at runtime.

## Verification
Host tests and GitHub Actions verify the camera path's periodic endpoints, render-state contracts, compilation, PNG dimensions, and package contents. Exact RSX appearance, frame pacing, and perceived loop smoothness still require a physical PS3 Slim / CFW 4.92 check.
