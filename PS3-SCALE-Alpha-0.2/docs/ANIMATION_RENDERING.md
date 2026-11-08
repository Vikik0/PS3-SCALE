# Aurora Core — Background Animation

## Visual design
PS3 SCALE uses a custom ambient 3D motif rather than copying the original PS3 XMB wave:
- a low-poly wireframe globe,
- three tilted orbital rings,
- two small orbiting markers,
- sixteen subtle drifting particles.

The globe sits toward the right side so the main menu remains readable. The colors stay within the existing cyan/blue/green palette.

## Performance constraints
- No textures are loaded or generated for the animation.
- No heap allocations occur in the frame loop.
- Geometry is low-poly and submitted as line primitives.
- Motion uses a small phase step (0.0012 per rendered frame) with slow secondary motion.
- The background is rendered once before page UI, so it continues behind Home, Scan, Details, Settings, Tools, About, the language picker, and the custom keyboard.
- The existing Home Animation setting disables the background completely.

## Verification
The GitHub Actions build verifies compilation and host-side tests. The exact appearance, frame pacing, and visual placement still require a real PS3 hardware check; CI cannot certify RSX presentation or perceived smoothness on the target television.
