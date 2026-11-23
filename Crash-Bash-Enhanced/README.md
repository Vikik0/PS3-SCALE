# Crash Bash Online — PlayStation 3

**PS3 Title ID:** `CBON00001` (four letters followed by five digits; consistent across the SFO, NPDRM Content ID and installed app path).

The installable release is **one unified PS3 application**. Its single XMB entry opens the native Tiny3D lobby; RetroArch and PCSX-ReARMed are embedded under the same title ID and launched by the lobby for local or online play. RetroArch Netplay handles gameplay synchronization.

This is an emulator-based homebrew package, not a native reimplementation of Crash Bash's original engine.

## What the PKG contains

- A root NPDRM `EBOOT.BIN` that starts the native Tiny3D lobby from the XMB.
- An embedded RetroArch executable at `USRDIR/retroarch/EBOOT.BIN`, signed for the same installed title ID and launched by the lobby.
- PCSX-ReARMed statically linked into the embedded RetroArch runtime, with its core metadata at `USRDIR/retroarch/cores/pcsx_rearmed_libretro_psl1ght.SELF`.
- RetroArch Netplay with public host announcements, NAT traversal and the Madrid relay endpoint enabled by default.
- Multitap on PS1 Port 1.
- Lockstep synchronization enabled for the large PS1 serialization state.
- An app-specific RetroArch profile so this package does not reuse another RetroArch installation's settings.
- A personal game-inclusive build can place the user-supplied BIN/CUE under `USRDIR/PSXISO/`; the lobby scans this installed folder first.
- No PSN initialization or PSN login.

The public CI package does **not** include the copyrighted Crash Bash disc image or a PlayStation BIOS. The personal build path can embed the user-supplied BIN/CUE locally without committing it to the public repository. A BIOS dumped from the player's own console is recommended; PCSX-ReARMed falls back to HLE when no recognized BIOS is present, which can reduce compatibility. See the [official PCSX-ReARMed documentation](https://docs.libretro.com/library/pcsx_rearmed/) for supported BIOS names and formats.

## Install and start the game

1. Install the single `Crash-Bash-Online-Personal-PS3.pkg` on a compatible jailbroken PS3 for the game-inclusive personal build.
2. In that build, the Crash Bash BIN/CUE is already installed under `/dev_hdd0/game/CBON00001/USRDIR/PSXISO/`; no external copy is needed. If you install the public CI package instead, copy your own CUE and referenced BIN tracks to `/dev_hdd0/PSXISO/` or a `/PSXISO/` folder on USB.
3. If using a BIOS, place the same BIOS file on each console in `/dev_hdd0/game/CBON00001/USRDIR/retroarch/system/`.
4. Launch the single **Crash Bash Online** icon from the XMB. The built-in lobby opens; choose **OPEN CRASH BASH ONLINE** for local play or **ONLINE ROOMS** for room-code matchmaking.
5. No second PKG or second XMB application is required.

The included core defaults to `Multitap Mode: Port 1`. PCSX-ReARMed supports Multitap and Netplay; its official documentation describes Port 1 Multitap as supporting up to five controller inputs. Crash Bash's own game limit still applies.

Settings, saves and logs are stored separately for this package under:

`/dev_hdd0/home/<user-id>/retroarch/CBON00001/`

The BIOS/system folder remains under the installed app directory shown above.

## Online play

1. All participating consoles install the same PKG.
2. Select the embedded Crash Bash CUE from the package's `USRDIR/PSXISO/` folder on each console. The public CI package instead requires the same external CUE and byte-identical BIN track files.
3. Use the same region, BIOS file (or all use HLE), core version and core options.
4. The host opens RetroArch's Netplay menu and selects **Start Netplay Host**.
5. Other players refresh the **Netplay Host List** and join the host.

### Automatic room-assisted launch (built into the same application)

The native Tiny3D lobby is the XMB entry point and is packaged together with RetroArch under the same title ID. Install the one unified PKG on every participating PS3. The room-code flow and automatic RetroArch host discovery are built in:

1. Install the same unified `CBON00001` package on every participating PS3.
2. Open **ONLINE ROOMS** in the built-in lobby. One player creates a room; the others join with its six-character invite code.
3. Every active player marks **READY**. Press **START** to open the CUE picker.
4. The host selects the same Crash Bash CUE used by everyone else and presses **CROSS**. The room is locked against late joins before RetroArch starts hosting.
5. Each player selects their CUE file and presses **CROSS** to confirm it. The companion fingerprints the CUE track layout and referenced track-file contents; the host waits until every active player has confirmed an identical disc before starting.
   - Changing the selected CUE, rescanning the disc list or backing out of the picker clears that player's previous confirmation. Toggling READY off also clears it; everyone must confirm again before the host can start.
   - While a room is waiting, READY expires after 90 seconds without a heartbeat. Heartbeats only refresh the seat and never restore READY. A returning player must explicitly press READY again and confirm the disc again before launch.
6. Each client then discovers the host's public RetroArch lobby entry and starts RetroArch with the authenticated relay endpoint and session ID.
7. If the client returns to the room after the match starts, the room displays **MATCH STARTED**; press **START** and select the same CUE to retry joining.

When using the integrated flow, players do not need to open **Netplay Host List** manually. The lobby API coordinates rooms and discovers the relay; RetroArch Netplay carries and synchronizes the game session. **OPEN CRASH BASH ONLINE** opens the embedded RetroArch menu for local play or manual Netplay.

The package is configured to allow up to three remote clients (four participants including the host). This is a configured maximum, **not a claim that four-player performance has already passed hardware testing**. Start with a two-player LAN test; four-player and internet tests are still release gates.

Lockstep is enabled because the pinned PCSX-ReARMed core reports a `0x440000`-byte savestate. Lockstep keeps one shared state instead of a rollback state for every buffered frame. This reduces memory use, but every frame waits for player input, so network latency can appear as input delay or brief stalls.

The build enables RetroArch's public lobby announcement, NAT traversal and the Madrid relay by default. If a direct connection is needed, forward **TCP port 55435** on the host router. Network restrictions or relay availability can still prevent a connection. Set a Netplay nickname before hosting so friends can identify the room. A Netplay Server Password blocks uninvited joins, but the room may still appear in the public host list. To keep it unlisted, disable **Publicly Announce Netplay** and give invited players the host's IP address.

**PSN is not used.** An internet connection is still required for online Netplay. The initial target is PS3-to-PS3; cross-platform sessions are not validated.

## Compatibility and testing status

The [official RetroArch PS1 guide](https://retroarch.com/ps1.html) identifies PCSX-ReARMed as the PS1 core that supports Netplay and Multitap. The [Netplay FAQ](https://docs.libretro.com/guides/netplay-faq/) also warns that PSX netplay can be too demanding. Lockstep is enabled as a mitigation, but it does not prove that this PS3 build has enough CPU headroom.

A successful CI build confirms compilation, package creation, SELF/SFO validation and checksum verification. It does **not** prove that a match runs at full speed or stays synchronized on physical PS3 consoles. Two-console LAN and internet tests, followed by four-player testing, remain necessary. See [HARDWARE_TEST_PLAN.md](HARDWARE_TEST_PLAN.md).

## PS3 launch fix

The single XMB entry now launches the native Tiny3D lobby. Its launcher starts the embedded static-core RetroArch executable from `USRDIR/retroarch/EBOOT.BIN`; both executables share the `CBON00001` title identity. The earlier separate-app install requirement is removed. See [LAUNCH_FIX.md](LAUNCH_FIX.md) for the launch rationale and remaining physical-console validation steps.

## Build and CI

The workflow is `.github/workflows/crash-bash-build.yml`. It uses pinned revisions of PCSX-ReARMed, RetroArch, RetroArch assets and core metadata.

The public workflow builds a pinned RetroArch + PCSX-ReARMed runtime, exports it under `USRDIR/retroarch/`, then builds the native Tiny3D lobby as the root XMB executable and packages both under one `CBON00001` title ID. It intentionally omits the copyrighted game dump from public CI artifacts.

The unified build:

- Builds the static PCSX-ReARMed core and links it into RetroArch.
- Configures Netplay lockstep, NAT traversal, and the Madrid relay.
- Uses `/dev_hdd0/home/<user-id>/retroarch/CBON00001/` for RetroArch settings, saves, and logs.
- Places the native lobby at the root `USRDIR/EBOOT.BIN` and the emulator at `USRDIR/retroarch/EBOOT.BIN`.
- Signs both executables for the same NPDRM Content ID so the lobby can launch the embedded runtime.
- Finalizes and verifies one `Crash-Bash-Online-Unified-PS3.pkg` with header revision `0x8000`, then generates its SHA-256 checksum and build manifest.
- Keeps the game dump out of the public workflow; a separate local personal-build script embeds the user's own BIN/CUE into the same single PKG.

The XMB icon uses custom Crash Bash Online cover art. The public CI artifact contains one installable emulator/lobby PKG plus its checksum and manifest. The personal game-inclusive PKG is built locally and is not uploaded to the public repository.

Documentation-only changes do not trigger a full PS3 cross-build. Host protocol tests and web-lobby security tests remain separate CI checks.

## Separate prototype components

The repository also contains an earlier native Tiny3D menu, a four-seat guest-room API backed by Vercel Blob, a JSON parser, a UDP input-packet codec, a UDP peer primitive, and PS1 disc/DAT extraction tools.

The web lobby supports up to four guest seats, public/private rooms, invite-code joins, and an ANY BATTLE action. A room becomes ready when at least two active players are all ready. Heartbeats keep active seats alive; browser refresh restores the same player identity; READY remains active only while its 90-second lease is refreshed, and inactive seats are pruned; and a membership change resets the remaining players' Ready flags. Rooms expire after 30 minutes without activity, expired Blob records are conditionally deleted, and public matchmaking follows Blob pagination beyond the first 100 rooms. The Vercel room API handles matchmaking and room membership only; it does not stream game state. The installed PS3 package hands gameplay synchronization to RetroArch Netplay. The repository's separate UDP input prototype is not used for match synchronization.

The native Tiny3D lobby is the primary executable in the unified `CBON00001` application. The embedded RetroArch runtime lives under `USRDIR/retroarch/` and is launched by the lobby; both are installed and updated by the same PKG. A ready room assigns one authoritative host; if that player leaves or expires, the first active player is promoted. Press **START** to open the CUE picker, which scans the embedded `USRDIR/PSXISO/` folder first, then the PS3 HDD and common USB `PSXISO` folders. The host launches the selected content with RetroArch Netplay hosting enabled. The room stores a separate random Netplay identity so the six-character private invite code is not exposed in RetroArch's public lobby. Immediately before the host starts RetroArch, the API locks the room against late joins and extends active player seats for a 30-minute match window; if RetroArch fails to start, the launcher rolls that lock back. Other players select the same CUE and press **CROSS**; the companion polls the RetroArch public lobby for the exact host nickname, retrieves the relay address and session ID through an authenticated room request, then starts RetroArch with `--connect` and `--mitm-session`. Direct connection is used only when the host is connectable and has no relay session. The client no longer needs to search Netplay Host List manually. The Vercel API does not carry game frames; match synchronization is provided by RetroArch Netplay. All players must use the same disc image, region, BIOS/HLE choice, core version and core options. The final synchronized match still requires validation on two physical PS3 consoles.

## Cross-play with crashbash.app

The website at https://crashbash.app is a separate browser port, not a PS1 emulator. It runs its own game simulation and multiplayer rooms. Its public documentation describes room codes, player seats, and controller inputs held by its server, but it does not publish a client API, wire protocol, or SDK for third-party consoles.

Consequently, the current PS3 RetroArch Netplay package and this repository's Vercel lobby cannot join rooms on crashbash.app. Matching the menu design or reusing a room code does not make the game protocols compatible. Cross-play requires the site owner's permission and a supported integration interface (or access to the relevant source/protocol), followed by a PS3 implementation of the same game/session client and mixed-device hardware tests. Do not advertise PS3-to-browser cross-play until that work is complete. See [CROSSPLAY_GATE.md](CROSSPLAY_GATE.md) for the permission requirements, integration paths, and hardware release tests.

## Personal game-inclusive build

The game files are never committed to this public repository or uploaded as public CI artifacts. To build one personal PKG containing the supplied disc dump:

1. Place exactly one legally obtained `.cue` file and all BIN tracks it references in `Crash-Bash-Enhanced/game/` (preserve the filenames referenced by the CUE).
2. Download the `Crash-Bash-RetroArch-Stage` artifact from a successful PS3 build workflow run and extract its contents directly into `Crash-Bash-Enhanced/pkgfiles/USRDIR/retroarch/`. Confirm that `EBOOT.BIN` and `cores/pcsx_rearmed_libretro_psl1ght.SELF` exist there.
3. Run `scripts/build_personal_pkg.sh` from a PS3DEV/PSL1GHT environment. The script validates the CUE references and sector alignment, stages the game under `USRDIR/PSXISO/`, builds one finalized PKG, and writes a checksum and manifest locally.

The public CI workflow remains game-free. The personal output is `build-artifacts/Crash-Bash-Online-Personal-PS3.pkg` and is not uploaded automatically.

## Licensing and project scope

No copyrighted Crash Bash game files, BIOS files or disc dumps are committed or distributed by the public CI workflow. The build includes the relevant upstream license files for RetroArch (GPL-3.0), PCSX-ReARMed (GPL-2.0) and RetroArch assets (CC BY 4.0).

This is an unofficial homebrew project and is not affiliated with Sony, Activision or the original Crash Bash developers.
