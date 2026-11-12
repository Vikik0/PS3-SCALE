# PS3 SCALE — Animated Staircase Background

## Visual design
The background follows the approved white / pale-lavender PS3 SCALE reference:
- a floating spiral staircase with 32 softly shaded treads,
- white top faces, pale-lilac risers and a subtle violet edge light,
- thin orbital light ribbons, a restrained halo and low-contrast particles,
- a slow day-to-dusk-to-night lighting cycle that never obscures the glass UI cards.

The Apollo Save logo remains a separate splash asset and is not recolored.

## Seamless motion
- The animation uses a fixed 21,600-frame cycle (six minutes at 60 FPS).
- Phase is calculated from `g_frame_counter % 21600`; no unbounded rotation or translation accumulates.
- Tread positions, the spiral edge, orbit markers, halo drift and lighting are all functions of the same periodic phase.
- The final phase wraps to the initial pose and light level without a video restart or hard cut.
- Home Animation can be disabled in Settings.

## Rendering correctness
- The staircase is real-time Tiny3D geometry, not a video.
- The 2D gradient is drawn first; the depth buffer is then cleared without clearing color before the 3D staircase pass. This prevents the full-screen background from hiding the staircase.
- RSX alpha testing and source-alpha blending are enabled for transparent bitmap glyphs, PNG cover art, glass cards and glow layers. This also prevents the black rectangles previously seen behind text.
- Stair tread faces, orbital rings and satellite lines set color on every vertex.
- The same scene is drawn behind Home, Scan, Details, Online DB, Settings, Tools, About, Help, Support, language selection and keyboard screens.
- No heap allocations, texture creation or file I/O occur in the frame loop.

## XMB artwork
- `ICON0.PNG` is the external PS3 SCALE application icon.
- `PIC1.PNG` is a bright white/lavender staircase wallpaper with quiet negative space for XMB icons.
- `PIC0.PNG` is the compact XMB preview tile.
- These are static package artwork; the in-app staircase is the animated Tiny3D scene.

## Verification
Host tests and GitHub Actions verify the render-state contracts, compilation, PNG dimensions and periodic phase formula. Exact RSX appearance, frame pacing and loop smoothness still require a physical PS3 Slim / CFW 4.92 check.
