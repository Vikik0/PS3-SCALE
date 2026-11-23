# Crash Bash Online — PlayStation 3

**PS3 Title ID:** `CBON00001` (four letters followed by five digits; consistent across the SFO, NPDRM Content ID and installed app path).

The current installable package is **RetroArch for PSL1GHT with PCSX-ReARMed statically linked as the PS1 core**. It runs the user's own Crash Bash disc dump and uses RetroArch Netplay for match synchronization.

This is an emulator-based homebrew package, not a native reimplementation of Crash Bash's original engine.

## What the PKG contains

- A direct NPDRM `EBOOT.BIN` that starts the RetroArch frontend; the PCSX-ReARMed core is statically linked into the same executable, so there is no Salamander second-stage launch.
- A matching core-info identity SELF at `USRDIR/cores/pcsx_rearmed_libretro_psl1ght.SELF`; it lets RetroArch match the core metadata and is not the XMB launch target.
- PCSX-ReARMed linked into RetroArch as the PS1 core.
- RetroArch Netplay with public host announcements, NAT traversal and the Madrid relay endpoint enabled by default.
- Multitap on PS1 Port 1.
- Lockstep synchronization enabled for the large PS1 serialization state.
- An app-specific RetroArch profile so this package does not reuse another RetroArch installation's settings.
- No PSN initialization or PSN login.

The package does **not** include the Crash Bash disc image or a PlayStation BIOS. Each player must provide their own legally obtained BIN/CUE dump. A BIOS dumped from the player's own console is recommended; PCSX-ReARMed falls back to HLE when no recognized BIOS is present, which can reduce compatibility. See the [official PCSX-ReARMed documentation](https://docs.libretro.com/library/pcsx_rearmed/) for supported BIOS names and formats.

## Install and start the game

1. Install `Crash-Bash-Online-PS3.pkg` on a compatible jailbroken PS3.
2. Create a BIN/CUE dump from your own Crash Bash disc.
3. Copy the CUE file and every referenced BIN track to `/dev_hdd0/PSXISO/` or a `/PSXISO/` folder on USB. Keep the CUE file and referenced filenames intact.
4. If using a BIOS, place the same BIOS file on each console in `/dev_hdd0/game/CBON00001/USRDIR/system/`.
5. Launch **Crash Bash Online** from the XMB, choose **Load Content**, and open the CUE file.
6. Optionally install the separate **Crash Bash Enhanced Lobby** PKG (`CBEN00001`) to coordinate room codes and Ready status before a match.

The included core defaults to `Multitap Mode: Port 1`. PCSX-ReARMed supports Multitap and Netplay; its official documentation describes Port 1 Multitap as supporting up to five controller inputs. Crash Bash's own game limit still applies.

Settings, saves and logs are stored separately for this package under:

`/dev_hdd0/home/<user-id>/retroarch/CBON00001/`

The BIOS/system folder remains under the installed app directory shown above.

## Online play

1. All participating consoles install the same PKG.
2. Load the exact same CUE file and byte-identical BIN track files on each console.
3. Use the same region, BIOS file (or all use HLE), core version and core options.
4. The host opens RetroArch's Netplay menu and selects **Start Netplay Host**.
5. Other players refresh the **Netplay Host List** and join the host.

### Automatic room-assisted launch (native lobby companion)

The separate **Crash Bash Enhanced Lobby** PKG (`CBEN00001`) provides an optional room-code flow and automatic RetroArch host discovery:

1. Install both `CBON00001` and `CBEN00001` on every participating PS3.
2. Open **ONLINE ROOMS** in the native companion. One player creates a room; the others join with its six-character invite code.
3. Every active player marks **READY**. Press **START** to open the CUE picker.
4. The host selects the same Crash Bash CUE used by everyone else and presses **CROSS**. The room is locked against late joins before RetroArch starts hosting.
5. Each player selects their CUE file and presses **CROSS** to confirm it. The companion fingerprints the CUE track layout and referenced track-file contents; the host waits until every active player has confirmed an identical disc before starting.
6. Each client then discovers the host's public RetroArch lobby entry and starts RetroArch with the authenticated relay endpoint and session ID.
7. If the client returns to the room after the match starts, the room displays **MATCH STARTED**; press **START** and select the same CUE to retry joining.

When using this assisted flow, players do not need to open **Netplay Host List** manually. The lobby API coordinates rooms and discovers the relay; RetroArch Netplay still carries and synchronizes the game session. The **OPEN CRASH BASH ONLINE** home-menu item opens the standalone RetroArch menu for local play or the manual Netplay flow above.

The package is configured to allow up to three remote clients (four participants including the host). This is a configured maximum, **not a claim that four-player performance has already passed hardware testing**. Start with a two-player LAN test; four-player and internet tests are still release gates.

Lockstep is enabled because the pinned PCSX-ReARMed core reports a `0x440000`-byte savestate. Lockstep keeps one shared state instead of a rollback state for every buffered frame. This reduces memory use, but every frame waits for player input, so network latency can appear as input delay or brief stalls.

The build enables RetroArch's public lobby announcement, NAT traversal and the Madrid relay by default. If a direct connection is needed, forward **TCP port 55435** on the host router. Network restrictions or relay availability can still prevent a connection. Set a Netplay nickname before hosting so friends can identify the room. A Netplay Server Password blocks uninvited joins, but the room may still appear in the public host list. To keep it unlisted, disable **Publicly Announce Netplay** and give invited players the host's IP address.

**PSN is not used.** An internet connection is still required for online Netplay. The initial target is PS3-to-PS3; cross-platform sessions are not validated.

## Compatibility and testing status

The [official RetroArch PS1 guide](https://retroarch.com/ps1.html) identifies PCSX-ReARMed as the PS1 core that supports Netplay and Multitap. The [Netplay FAQ](https://docs.libretro.com/guides/netplay-faq/) also warns that PSX netplay can be too demanding. Lockstep is enabled as a mitigation, but it does not prove that this PS3 build has enough CPU headroom.

A successful CI build confirms compilation, package creation, SELF/SFO validation and checksum verification. It does **not** prove that a match runs at full speed or stays synchronized on physical PS3 consoles. Two-console LAN and internet tests, followed by four-player testing, remain necessary. See [HARDWARE_TEST_PLAN.md](HARDWARE_TEST_PLAN.md).

## PS3 launch fix

The XMB entry now launches the static-core RetroArch executable directly through a finalized NPDRM EBOOT. The earlier Salamander second-stage spawn has been removed. See [LAUNCH_FIX.md](LAUNCH_FIX.md) for the rationale and the remaining physical-console validation steps.

## Build and CI

The workflow is `.github/workflows/crash-bash-build.yml`. It uses pinned revisions of PCSX-ReARMed, RetroArch, RetroArch assets and core metadata.

The build script is `scripts/build_online_pkg_clean.sh`. It:

- Patches the PCSX-ReARMed Multitap default to Port 1.
- Enables RetroArch Netplay lockstep, NAT traversal and the Madrid relay.
- Isolates the PS3 user profile under `retroarch/CBON00001`.
- Builds the PS1 core, links it into RetroArch, and creates a direct NPDRM `EBOOT.BIN` from that executable.
- Validates the direct EBOOT SELF header, static libretro entry points and SFO app identity.
- Finalizes the PKG with `package_finalize` and requires header revision `0x8000` before upload.
- Produces a SHA-256 checksum and build manifest.

Artifacts from workflow runs before the PKG-finalization fix contain revision `0x0000` (debug/non-finalized). Use only an artifact from a run after the finalization fix; the CI workflow now rejects a non-finalized package.

The XMB icon uses custom Crash Bash Online cover art.\n\nThe CI artifact contains:

- `Crash-Bash-Online-PS3.pkg`
- `Crash-Bash-Online-PS3.pkg.sha256`
- `BUILD-MANIFEST.txt`

Verify the downloaded package with:

`sha256sum -c Crash-Bash-Online-PS3.pkg.sha256`

Documentation-only changes no longer trigger a full PS3 cross-build. Host protocol tests and web-lobby security tests remain separate CI checks.

## Separate prototype components

The repository also contains an earlier native Tiny3D menu, a four-seat guest-room API backed by Vercel Blob, a JSON parser, a UDP input-packet codec, a UDP peer primitive, and PS1 disc/DAT extraction tools.

The web lobby supports up to four guest seats, public/private rooms, invite-code joins, and an ANY BATTLE action. A room becomes ready when at least two active players are all ready. Heartbeats keep active seats alive; browser refresh restores the same player identity and ready state; inactive seats are pruned; and a membership change resets the remaining players' Ready flags. Rooms expire after 30 minutes without activity, expired Blob records are conditionally deleted, and public matchmaking follows Blob pagination beyond the first 100 rooms. The Vercel room API handles matchmaking and room membership only; it does not stream game state. The installed PS3 package hands gameplay synchronization to RetroArch Netplay. The repository's separate UDP input prototype is not used for match synchronization.

The native Tiny3D lobby companion has its own installable PKG target (`CBEN00001`) and CI build artifact. It uses the production room API at `https://crash-bash-enhanced.vercel.app` and is installed separately from the RetroArch game package (`CBON00001`). A ready room assigns one authoritative host; if that player leaves or expires, the first active player is promoted. Press **START** to open the CUE picker, which scans the PS3 HDD and common USB `PSXISO` folders. The host launches the selected content with RetroArch Netplay hosting enabled. The room stores a separate random Netplay identity so the six-character private invite code is not exposed in RetroArch's public lobby. Immediately before the host starts RetroArch, the API locks the room against late joins and extends active player seats for a 30-minute match window; if RetroArch fails to start, the launcher rolls that lock back. Other players select the same CUE and press **CROSS**; the companion polls the RetroArch public lobby for the exact host nickname, retrieves the relay address and session ID through an authenticated room request, then starts RetroArch with `--connect` and `--mitm-session`. Direct connection is used only when the host is connectable and has no relay session. The client no longer needs to search Netplay Host List manually. The home menu also has **OPEN CRASH BASH ONLINE** for local offline launching. The Vercel API does not carry game frames; match synchronization is provided by RetroArch Netplay. All players must use the same disc image, region, BIOS/HLE choice, core version and core options. The final synchronized match still requires validation on two physical PS3 consoles.

## Cross-play with crashbash.app

The website at https://crashbash.app is a separate browser port, not a PS1 emulator. It runs its own game simulation and multiplayer rooms. Its public documentation describes room codes, player seats, and controller inputs held by its server, but it does not publish a client API, wire protocol, or SDK for third-party consoles.

Consequently, the current PS3 RetroArch Netplay package and this repository's Vercel lobby cannot join rooms on crashbash.app. Matching the menu design or reusing a room code does not make the game protocols compatible. Cross-play requires the site owner's permission and a supported integration interface (or access to the relevant source/protocol), followed by a PS3 implementation of the same game/session client and mixed-device hardware tests. Do not advertise PS3-to-browser cross-play until that work is complete. See [CROSSPLAY_GATE.md](CROSSPLAY_GATE.md) for the permission requirements, integration paths, and hardware release tests.

## Licensing and project scope

No copyrighted Crash Bash game files, BIOS files or disc dumps are committed. The build includes the relevant upstream license files for RetroArch (GPL-3.0), PCSX-ReARMed (GPL-2.0) and RetroArch assets (CC BY 4.0).

This is an unofficial homebrew project and is not affiliated with Sony, Activision or the original Crash Bash developers.
