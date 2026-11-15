# PS3 SCALE — Visual identity (approved reference)

## Direction
The approved user reference is **pearl-white and silver**, with a sweeping
spiral staircase, faint lavender atmospheric lighting and **violet only as the
interaction/accent color**. Do not replace it with a dark cyberpunk corridor,
a blue tunnel or distracting motion graphics. This is a PS3 application, so
geometry, memory and texture use must remain conservative.

## Colors
- Background pearl: `#FCFCFF`; silver-lavender: `#E8E5F0`.
- Primary ink: `#29243F`; secondary ink: `#514B69`.
- Brand violet / selected focus: `#875BE8`.
- Pale selected glass: `#F4EFFF` at controlled transparency.
- Retain good contrast for all foreground text, especially on white cards.

## Screens and layout
The approved collage has Splash, Home, Game Library, Game Details, Online DB,
Settings and Help. Preserve the existing **five Home cards**: Scan Games,
Online DB, Settings, Help and About. Never add random feature panels over
the artwork. The top-right hardware badges are part of the reference, but
hardware values should be read from detected information where available
rather than treated as an absolute compatibility guarantee.

## XMB artwork
`scripts/generate_xmb_assets.py` generates the real installation assets:
- `ICON0.PNG` 320×176: deep-purple premium icon with white steps and two-line
  PS3 / SCALE wordmark.
- `PIC0.PNG` 310×176: pale alternate preview.
- `PIC1.PNG` 1280×720: pearl-white, softly shaded staircase wallpaper
  with open space for XMB labels.

All XMB PNGs are **static**. CI publishes the three actual generated PNGs in
`PS3-SCALE-XMB-Pearl-Design-Preview` alongside the PS3 package. Binary PNGs
already tracked in the source tree can be older than the files produced by
`make pkg`; the generated build artifacts are the authoritative previews.

## In-app icons and cards
Home tiles use low-cost Tiny3D vector geometry: gamepad, three-layer online
database, gear, help and information. Selected tile uses a deeper violet
plate with a pale white symbol. Glass cards and settings/filter chips use
rounded corners, a soft drop shadow and a thin selected outline. Controls
must work with the original PS3 D-Pad/CROSS/CIRCLE bindings.

## Motion
32 visible treads scroll past a camera in a repeating 18-tread helix.
The 21,600-frame cycle is a continuous virtual ascent (not a reversible
side-to-side camera dolly), giving a repeating staircase without a visible
restart. The rendering is procedural perspective projected through Tiny3D;
it is **not yet** the physically detailed photographic staircase of the
reference image. Moving to photoreal textures and improved material lighting
is a separate measured performance milestone.

## Quality gate
- Preserve game discovery, online databases, PS3 / PS2 profile safeguards,
  music and support features.
- Build ELF, SELF, PKG and GNPDRM PKG; run UI / scan / patch-safety tests.
- Inspect PNG previews, verify text/handrail legibility and actual frame
  pacing on a PS3 before promoting this branch to a release.
