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
- The reference background remains pearl-white, silver and pale lavender, with thin violet illumination. The procedural stairs are a stylized, low-memory approximation; they are not yet photorealistic.
- The camera travels straight ahead through an **endlessly repeating staircase**, with a camera-relative window of 32 visible treads.
- The helix repeats its angle every 18 treads. A continuous fractional camera position moves upward through those steps; at each whole-tread boundary, the tread window shifts by one index. The same positions remain in view, so no visible camera teleport or reverse is required.
- A complete visual revolution advances over 21,600 frames (six minutes **if** the PS3 sustains 60 FPS). This is currently frame-based timing, not real-time clock timing, so actual speed depends on measured frame rate.
- The background lighting and particles use the same periodic phase. The player can disable animation in Settings.

## Pearl material upgrade (visual stage 2)
- A thin **polished bevel** is now drawn along the outside of each tread, with
  a matching **contact-shadow strip** underneath the riser. Together they add
  visible thickness while retaining white/pearl architecture.
- Each projected face carries four tinted vertex colors for a restrained
  reflection gradient and depth haze. The RSX interpolates these vertices;
  this is geometry shading, **not** ray tracing, bump mapping or a 4K texture.
- Riser and underside wireframes are intentionally hidden to avoid the
  technical/cartoony outlined look. Bright top edges remain visible.
- The scene stays within five surfaces per 32-tread window and uses the
  existing batched Tiny3D render path.
- `scripts/preview_staircase_materials.py` generates five approximate
  **software previews** plus a contact sheet during CI. These are NOT actual
  PS3 screenshots or a guarantee of real RSX material appearance.
- The user-approved photo-like staircase is still a *target aesthetic*.
  Captured PS3 screenshots, real frame pacing, alpha behavior, and the loop
  boundary must be checked before merging this prototype.

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
