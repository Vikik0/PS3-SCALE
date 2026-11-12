# GitHub community game search

PS3 SCALE indexes public NCL files from these GitHub repositories:

- Nascar1243/PS3-FPS-Patches — FPS patch files described by their maintainer as tested on real PS3 hardware.
- DoSpamu/RPCS3toArtemisPatches — community-confirmed and broader Artemis/RPCS3-converted files. Unverified USERLIST records remain labelled as unverified.
- RobertoTorino/ArtemisPS3Offline — offline Artemis code database.
- PS3-Pro/Game-Cheats — community cheat files, including files named by Title ID only.

The index is refreshed weekly by GitHub Actions and can also be refreshed manually. When Online Graphics Database opens, PS3 SCALE compares installed PS3 game names and Title IDs against the downloaded index. Matching files are shown even when they are not part of PS3 SCALE's small, exact-version graphics-profile catalogue.

## Safety boundary

- A listed NCL file is a community reference, not a verified PS3 SCALE graphics profile.
- NCL files can contain cheats, FPS changes, debug codes, or RPCS3-converted data. They are not all graphics enhancements and are not all tested on real PS3 hardware.
- PS3 SCALE does not execute, install, or auto-apply community NCL files.
- The existing one-launch profile system remains separate and continues to require an exact Title ID + APP_VER match and explicit confirmation.
- Review the repository, file, game version, and hardware compatibility before using a community file with Artemis.

The index only covers the public repositories listed above. A game with no matching file in those sources is shown as having no indexed community file; that does not prove that no patch exists anywhere on GitHub.
