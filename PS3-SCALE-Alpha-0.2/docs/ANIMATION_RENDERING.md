# PS3 SCALE — Seamless Ascending Spiral Staircase

## Visual design
The in-app background follows the approved pearl-white / pale-lavender PS3 SCALE reference:
- a wide floating spiral staircase with a 48-tread visible window and 24 treads per full revolution,
- soft white and silver tread tops, pale-lilac risers, and restrained violet edge lighting,
- inner and outer spiral handrails with fine balusters,
- a quiet lavender gradient, subtle light ribbons, a restrained halo, and low-contrast particles,
- background contrast kept low enough for the glass UI cards and game covers to remain readable.

The Apollo Save logo remains a separate splash asset and is not recolored.

## Camera motion and seamless loop
- The camera ascends in a straight line through the open central core of the spiral staircase; it does not reverse or visibly pause.
- The staircase is treated as a repeating helical environment. A moving 48-tread window is generated relative to the camera, with 24 treads forming one complete revolution.
- The camera advances exactly one 24-tread revolution per 21,600-frame cycle (six minutes at 60 FPS).
- The cycle counter advances the tread indices while the angular position wraps every 24 treads. Relative height is calculated from the local step offset, so large world coordinates do not accumulate.
- At the cycle boundary, the camera has advanced one full revolution and the same relative staircase geometry is visible again. The loop does not depend on restarting a video or reversing the camera.
- The background light phase uses the same period, so the staircase, glow, and lighting return to their initial state together.
- Home Animation can be disabled in Settings.

## Rendering and performance
- The staircase is procedural 3D geometry with a perspective camera transform, projected in software into Tiny3D's stable 2D rendering pass. This deliberately avoids relying on RSX depth-buffer behavior that previously hid the scene.
- Faces are sorted back-to-front; faces and line segments crossing the near plane are skipped rather than stretched across the screen.
- The inner and outer rails, balusters, and tread-edge highlights are submitted in a small number of batched line draws.
- RSX alpha testing and source-alpha blending remain enabled for transparent bitmap glyphs, PNG cover art, glass cards, and glow layers.
- The same scene is drawn behind Home, Scan, Details, Online DB, Settings, Tools, About, Help, Support, language selection, and keyboard screens.
- No heap allocations, texture creation, or file I/O occur in the frame loop.

## XMB artwork
- `ICON0.PNG` is the external PS3 SCALE application icon.
- `PIC1.PNG` is the bright white/lavender staircase wallpaper with quiet negative space for XMB icons.
- `PIC0.PNG` is the compact XMB preview tile.
- These are static package artwork; the in-app staircase is generated and animated at runtime.

## Verification
Host tests and GitHub Actions verify the camera-loop formula, render-state contracts, compilation, PNG dimensions, and package contents. Exact RSX appearance, frame pacing, and perceived loop smoothness still require a physical PS3 Slim / CFW 4.92 check.
