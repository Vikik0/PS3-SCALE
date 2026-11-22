# Crash Bash Online — PlayStation 3

The current installable package is **RetroArch for PSL1GHT with PCSX-ReARMed statically linked as the PS1 core**. It runs the user's own Crash Bash disc dump and uses RetroArch Netplay for match synchronization.

This is an emulator-based homebrew package, not a native reimplementation of Crash Bash's original engine.

## What the PKG contains

- A PS3 XMB launcher (Salamander) and RetroArch frontend.
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
4. If using a BIOS, place the same BIOS file on each console in `/dev_hdd0/game/CBASH0001/USRDIR/system/`.
5. Launch **Crash Bash Online** from the XMB, choose **Load Content**, and open the CUE file.

The included core defaults to `Multitap Mode: Port 1`. PCSX-ReARMed supports Multitap and Netplay; its official documentation describes Port 1 Multitap as supporting up to five controller inputs. Crash Bash's own game limit still applies.

Settings, saves and logs are stored separately for this package under:

`/dev_hdd0/home/<user-id>/retroarch/CBASH0001/`

The BIOS/system folder remains under the installed app directory shown above.

## Online play

1. All participating consoles install the same PKG.
2. Load the exact same CUE file and byte-identical BIN track files on each console.
3. Use the same region, BIOS file (or all use HLE), core version and core options.
4. The host opens RetroArch's Netplay menu and selects **Start Netplay Host**.
5. Other players refresh the **Netplay Host List** and join the host.

The package is configured to allow up to three remote clients (four participants including the host). This is a configured maximum, **not a claim that four-player performance has already passed hardware testing**. Start with a two-player LAN test; four-player and internet tests are still release gates.

Lockstep is enabled because the pinned PCSX-ReARMed core reports a `0x440000`-byte savestate. Lockstep keeps one shared state instead of a rollback state for every buffered frame. This reduces memory use, but every frame waits for player input, so network latency can appear as input delay or brief stalls.

The build enables RetroArch's public lobby announcement, NAT traversal and the Madrid relay by default. If a direct connection is needed, forward **TCP port 55435** on the host router. Network restrictions or relay availability can still prevent a connection. Set a Netplay nickname before hosting so friends can identify the room. A Netplay Server Password blocks uninvited joins, but the room may still appear in the public host list. To keep it unlisted, disable **Publicly Announce Netplay** and give invited players the host's IP address.

**PSN is not used.** An internet connection is still required for online Netplay. The initial target is PS3-to-PS3; cross-platform sessions are not validated.

## Compatibility and testing status

The [official RetroArch PS1 guide](https://retroarch.com/ps1.html) identifies PCSX-ReARMed as the PS1 core that supports Netplay and Multitap. The [Netplay FAQ](https://docs.libretro.com/guides/netplay-faq/) also warns that PSX netplay can be too demanding. Lockstep is enabled as a mitigation, but it does not prove that this PS3 build has enough CPU headroom.

A successful CI build confirms compilation, package creation, SELF/SFO validation and checksum verification. It does **not** prove that a match runs at full speed or stays synchronized on physical PS3 consoles. Two-console LAN and internet tests, followed by four-player testing, remain necessary. See [HARDWARE_TEST_PLAN.md](HARDWARE_TEST_PLAN.md).

## Build and CI

The workflow is `.github/workflows/crash-bash-build.yml`. It uses pinned revisions of PCSX-ReARMed, RetroArch, RetroArch assets and core metadata.

The build script is `scripts/build_online_pkg.sh`. It:

- Patches the PCSX-ReARMed Multitap default to Port 1.
- Enables RetroArch Netplay lockstep, NAT traversal and the Madrid relay.
- Isolates the PS3 user profile under `retroarch/CBASH0001`.
- Builds the PS1 core and Salamander EBOOT, then links the real core into RetroArch.
- Validates the SELF headers, SFO app identity and PKG header.
- Produces a SHA-256 checksum and build manifest.

The CI artifact contains:

- `Crash-Bash-Online-PS3.pkg`
- `Crash-Bash-Online-PS3.pkg.sha256`
- `BUILD-MANIFEST.txt`

Verify the downloaded package with:

`sha256sum -c Crash-Bash-Online-PS3.pkg.sha256`

Documentation-only changes no longer trigger a full PS3 cross-build. Host protocol tests and web-lobby security tests remain separate CI checks.

## Separate prototype components

The repository also contains an earlier native Tiny3D menu, a guest-room API backed by Vercel Blob, a JSON parser, a UDP input-packet codec, a UDP peer primitive, and PS1 disc/DAT extraction tools.

Those components remain development prototypes. The current installable RetroArch PKG does **not** use the Vercel room API or custom UDP peer code for gameplay. Actual match synchronization in this package is provided by RetroArch Netplay. The web room service and custom transport must not be described as a completed game-session path.

## Licensing and project scope

No copyrighted Crash Bash game files, BIOS files or disc dumps are committed. The build includes the relevant upstream license files for RetroArch (GPL-3.0), PCSX-ReARMed (GPL-2.0) and RetroArch assets (CC BY 4.0).

This is an unofficial homebrew project and is not affiliated with Sony, Activision or the original Crash Bash developers.
