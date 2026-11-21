# Online enhancement catalogue — Alpha 0.23

## Data sources

PS3 SCALE reads two separate community datasets:

- `data/online_profiles.tsv` — version-matched memory profiles used by the FPS Control screen.
- `data/community_game_files.tsv` — metadata for community NCL files. Only entries categorized as `GRAPHICS_FPS` are eligible for the Artemis install action.

The catalogue is community-maintained. A title name is not enough to install a patch; the installed game identity and update version must match exactly.

## Online DB controls and refresh

- The first time Online DB is opened in a session, PS3 SCALE starts a real HTTPS refresh automatically while keeping the last valid local cache visible.
- **Triangle** forces a fresh profile + community-index download. **Start** remains an alternate refresh shortcut.
- **Select** opens the search keyboard.
- **Square** cycles the result filter. The default is Graphics + Any Region/Version so available candidates appear without cycling the filter first. A non-exact candidate is shown for review only; it is not installable.
- **Cross** installs an eligible NCL file to Artemis, or opens the selected game details when the result is a profile rather than a community file.
- Online DB remains browsable from the last valid cache if the console is offline.

The HTTPS catalogue response is size-limited and parsed before replacing the previous cache. A failed refresh does not discard a valid cached dataset. No PSN account is required for this HTTPS catalogue.

## NCL installation to Artemis

The Cross install action is deliberately narrow:

1. The result must be a `GRAPHICS_FPS` community entry with a `.ncl` extension.
2. The matching PS3 game must already be scanned from an HDD path under `/dev_hdd0/`.
3. The installed game's Title ID and APP_VER must exactly match the community entry.
4. Artemis PS3 must be installed. PS3 SCALE detects the upstream `ARTPS3001` package ID and the Artemis R5 `ARTZ00001` package ID, then selects that installation's USERLIST directory.
5. The source repository, branch and path must pass path validation. The downloaded file is limited to 2 MiB, must be plain text, and ELF/PKG/binary content is rejected. Its actual byte size must also match the size recorded in the refreshed index; a changed upstream file is rejected until the index is refreshed.
6. Existing destination files are never overwritten.

Accepted files are saved to the detected Artemis installation:

- `/dev_hdd0/game/ARTPS3001/USRDIR/USERLIST/` for the upstream package.
- `/dev_hdd0/game/ARTZ00001/USRDIR/USERLIST/` for Artemis R5 builds using that package ID.

Temporary cleanup checks both supported USERLIST locations. The filename is prefixed with `PS3SCALE_<TITLE_ID>_<APP_VER>_` so the target game and version are visible in Artemis' user list.

PS3 SCALE does **not** execute an NCL file or apply it automatically. After installation, open Artemis PS3 and select the NCL entry for the matching game. The community file may contain in-game memory codes; review the source and use it at your own discretion. PS3 SCALE does not modify the original game folder, EBOOT, update data or ISO.

### Temporary or permanent storage

Settings → **Enhancement Storage** controls future NCL downloads:

- **Temporary** is the default. PS3 SCALE records each file it installs and removes those files the next time PS3 SCALE starts.
- **Permanent** leaves newly installed NCL files in Artemis' USERLIST until the user removes them.

Temporary cleanup is intentionally performed on the next PS3 SCALE launch, not while Artemis may still need to read the file. Changing this setting does not retroactively change files already installed.

## Version-matched FPS profiles

- A profile matches only when both Title ID and APP_VER are exact.
- A one-launch profile is armed only after PS3 SCALE revalidates the exact installed or mounted game and the user confirms.
- Multi-write and manual-only profiles remain excluded from automatic application.
- The generated webMAN script is one-launch only. It checks the Title ID, in-game state, target address and expected original bytes before attempting a memory write. PS3 SCALE does not claim success without a verified post-write value.
- If PS3MAPI is unavailable, PS3 SCALE removes its own one-launch profile script and still asks webMAN to start the game unmodified. A missing PS3MAPI endpoint no longer blocks normal game launch.
- If webMAN's local HTTP endpoint cannot be reached or rejects the request, the UI reports the specific network/HTTP failure instead of silently treating it as a game-file issue.

## Game launch transport

PS3 SCALE loads the PS3 NET and NETCTL modules, initializes the network control layer, sends the bounded local HTTP request to webMAN MOD on `127.0.0.1:80`, then closes the socket and releases the modules. Launching is performed on a worker thread so the UI remains responsive.

- Installed HDD/NPDRM titles use `/play.ps3?<TITLE_ID>`.
- PS3 folder/ISO titles use `/play.ps3<URL-encoded-path>`.
- PS2 Classics must be started through their compatible launcher.

## Known limitations

- The community index and NCL files are community-supplied, not independently hardware-verified by PS3 SCALE.
- Exact Title ID + APP_VER matching reduces cross-version mistakes but cannot prove that a community patch is safe or stable on every console.
- The NCL installer accepts only text files tagged `GRAPHICS_FPS`; it does not install arbitrary binaries, EBOOT replacements, PKG files or general `.txt` downloads.
- No universal upscaler, frame generation or guaranteed 60 FPS is provided.
- A future binary patch installer would need cryptographic hashes, source/license metadata, backups, restore and rollback support before it could be enabled.
