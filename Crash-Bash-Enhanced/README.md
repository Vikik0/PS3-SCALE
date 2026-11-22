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

The repository also contains an earlier native Tiny3D menu, a guest-room API backed by Vercel Blob, a JSON parser, a UDP input-packet codec, a UDP peer primitive, and PS1 disc/DAT extraction tools.

Those components remain development prototypes. The current installable RetroArch PKG does **not** use the Vercel room API or custom UDP peer code for gameplay. Actual match synchronization in this package is provided by RetroArch Netplay. The web room service and custom transport must not be described as a completed game-session path.

## Licensing and project scope

No copyrighted Crash Bash game files, BIOS files or disc dumps are committed. The build includes the relevant upstream license files for RetroArch (GPL-3.0), PCSX-ReARMed (GPL-2.0) and RetroArch assets (CC BY 4.0).

This is an unofficial homebrew project and is not affiliated with Sony, Activision or the original Crash Bash developers.
