# PS3 SCALE — Seamless Staircase Background

## Visual design
The background follows the approved white / pale-lavender PS3 SCALE reference:
- a floating spiral staircase with 22 softly shaded treads,
- white top faces, pale-lilac risers and a subtle violet edge light,
- thin orbital light ribbons, a restrained halo and a few low-contrast particles,
- a day-to-dusk-to-night lighting cycle that never obscures the glass UI cards.

The user-supplied green Apollo Save logo is a separate splash asset. It is not recolored or redrawn.

## Seamless motion
- The animation uses a fixed 21,600-frame cycle (six minutes at 60 FPS).
- Phase is calculated from \`g_frame_counter % 21600\`; it never accumulates unbounded translation or rotation.
- Stair positions, small tread offsets, orbit markers, halo drift and day/night lighting are all functions of the same periodic phase.
- The final phase wraps to the initial pose and light level. There is no video end, restart, or hard cut.
- The initial phase is bright daytime. The night phase is a restrained violet, while foreground cards stay pale for readability.

## Rendering and performance
- The staircase is real-time Tiny3D geometry; no background video is decoded.
- The scene is drawn before page content on Home, Scan, Details, Online DB, Settings, Tools, About, Help, Support, language selection and keyboard screens.
- The Home Animation setting disables the scene and leaves the static pale-lavender background.
- No heap allocations, texture creation or file I/O occur in the frame loop.
- Geometry remains deliberately low-complexity for PS3 Slim RSX.

## XMB artwork
- \`ICON0.PNG\`, \`PIC1.PNG\` and \`PIC0.PNG\` are generated as static bright staircase artwork at their PS3 dimensions.
- XMB artwork is not a looping video. The moving staircase belongs to the in-app Tiny3D renderer.

## Verification
Host tests and GitHub Actions verify compilation, asset dimensions, the periodic phase formula and page-level rendering calls. The exact RSX appearance, alpha blending, frame pacing and perceived loop smoothness still require a real PS3 Slim / CFW 4.92 hardware check.
