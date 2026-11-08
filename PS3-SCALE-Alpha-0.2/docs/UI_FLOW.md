# PS3 SCALE UI flow — requested product behavior

## Main menu

The application opens to three primary destinations, each with a native Tiny3D vector icon:
1. Scan Games — magnifying glass / library search icon.
2. Settings — gear icon.
3. About PS3 SCALE — information-in-circle icon.

The icons are drawn from Tiny3D line and quad primitives, so they do not depend on external PNG assets or a font glyph being present.

## Scan Games

- Separate PS3 and PS2 library views.
- Read-only scan of the internal HDD, mounted Blu-ray and USB devices.
- Search by game title using the PS3 system on-screen keyboard (SELECT opens it).
- Display an organized list with platform/source metadata.
- Selecting a title opens a dedicated details screen.
- Local ICON0.PNG artwork is discovered for PS3 folder games and installed titles and rendered through YA2D. ISO cover extraction and remote cover lookup remain planned.

## Game details

The page identifies the selected title using its exact PS3 Title ID or PS2 disc serial where available. It must show:
- game title, platform and source path;
- exact identity and update version when known;
- current enhancement profile and trust level;
- whether a verified profile exists for this exact game;
- backup and rollback status;
- a clear distinction between preview, downloaded, prepared and actually applied states.

No "Applied" status may be displayed merely because a profile was downloaded or staged.

## Settings

The user-supplied GTA V Greyhound pause-menu remix is the requested startup theme. The current Alpha build displays its intended selection, but MP3 playback is not yet wired into the PS3 audio backend.

## Current implementation boundary

The home menu, including native vector icons for Scan Games, Settings and About, PS3/PS2 scan pages, title search through the PS3 OSK, game details, Settings and About pages are implemented in the UI source. Local ICON0 covers are supported for folder/installed titles. ISO/online cover lookup, remote catalog integration, profile application and actual audio playback remain pending and must be built/tested separately.
