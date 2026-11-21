# GitHub community game search

PS3 SCALE indexes public NCL files from these GitHub repositories:

- Nascar1243/PS3-FPS-Patches — FPS patch files described by their maintainer as tested on real PS3 hardware.
- DoSpamu/RPCS3toArtemisPatches — community-confirmed and broader Artemis/RPCS3-converted files. Unverified USERLIST records remain labelled as unverified.
- RobertoTorino/ArtemisPS3Offline — offline Artemis code database.
- PS3-Pro/Game-Cheats — community cheat files, including files named by Title ID only.

The app refreshes the profile and community index automatically the first time Online DB is opened. Triangle forces a new HTTPS refresh; Start is an alternate refresh shortcut. Select opens search and Square changes the result filter. The default filter is Graphics + Any Region/Version so candidates appear without cycling Square before searching.

## Install behavior

Cross can install a community file only when all checks pass:

- The index category is `GRAPHICS_FPS`.
- The file extension is `.ncl`.
- The selected local PS3 game is on `/dev_hdd0/`.
- The game's Title ID and APP_VER exactly match the entry.
- Artemis PS3 is installed. Both the upstream `ARTPS3001` and Artemis R5 `ARTZ00001` package IDs are supported.
- The GitHub repository, branch and path are safe; the downloaded file is at most 2 MiB and contains plain text rather than ELF/PKG/binary data.
- Existing destination files are never overwritten.

The file is placed in the detected Artemis `USRDIR/USERLIST/` directory. Temporary is the default storage mode and deletes PS3 SCALE-managed files on the next PS3 SCALE start. Permanent mode retains new files until the user removes them.

PS3 SCALE does **not** execute or auto-apply the NCL file. The user must open Artemis and select the matching code. NCL files can contain memory-writing codes, so review the source and version before using them.

## Safety boundary

- A listed NCL file is a community reference, not a verified PS3 SCALE graphics profile.
- NCL files can contain cheats, FPS changes, debug codes, or RPCS3-converted data. They are not all graphics enhancements and are not all tested on real PS3 hardware.
- PS3 SCALE does not install arbitrary binaries, EBOOT replacements, PKG files or generic `.txt` downloads.
- The existing one-launch profile system remains separate and continues to require an exact Title ID + APP_VER match and explicit confirmation.
- Review the repository, file, game version, and hardware compatibility before using a community file with Artemis.

The index only covers the public repositories listed above. A game with no matching file in those sources is shown as having no indexed community file; that does not prove that no patch exists anywhere on GitHub.
