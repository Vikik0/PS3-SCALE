# Crash Bash Online — PlayStation 3

This project packages a PS3 build of **RetroArch Netplay with PCSX-ReARMed statically linked**. The goal is to let players run their own Crash Bash disc dump and use RetroArch's existing synchronized netplay instead of trying to reproduce the original game engine.

## What the current PKG contains

- A PSL1GHT-based PS3 launcher (Salamander) and RetroArch frontend.
- PCSX-ReARMed linked into the frontend as the PS1 emulator core.
- RetroArch Netplay with public host announcements, NAT traversal and the Madrid relay endpoint enabled by default.
- Multitap configured on PS1 Port 1; the host accepts up to three remote clients (four network players total).
- No PSN initialization or PSN login.
- The original Crash Bash disc image and PlayStation BIOS are **not** included.

The PKG is an emulator and online-play setup, not a redistributed copy of Crash Bash. Each player must supply their own legally obtained BIN/CUE dump. A BIOS dumped from the player's own console is recommended.

## Install and start the game

1. Install `Crash-Bash-Online-PS3.pkg` on a compatible jailbroken PS3.
2. Create a BIN/CUE dump from your own Crash Bash disc.
3. Copy both files to `/dev_hdd0/PSXISO/` or to a `/PSXISO/` folder on USB. Keep the BIN filename referenced by the CUE file correct.
4. If using a PlayStation BIOS, place the matching BIOS file in `/dev_hdd0/game/CBASH0001/USRDIR/system/` on each console. The BIOS is not bundled.
5. Launch **Crash Bash Online** from the XMB, choose **Load Content**, and open the CUE file.

The package sets PCSX-ReARMed's `Multitap Mode` to `Port 1`. Keep the game region, disc dump, BIOS choice, core version and core options identical on every console.

## Online play

1. All players install the same PKG and load the same Crash Bash version.
2. The host opens RetroArch's Netplay menu and selects **Start Hosting**.
3. Other players refresh the **Netplay Host List** and join the host.
4. The host can accept up to three remote connections, for four players total.

The build enables RetroArch's public lobby announcement, NAT traversal and the Madrid relay by default. A direct connection can also use TCP/UDP port `55435`; forward that port on the host router if needed. Strict NAT, firewall rules or relay availability can still prevent a connection.

**PSN is not used.** An internet connection is still required for online Netplay.

## Build and CI

The installable package is built by `.github/workflows/crash-bash-build.yml` using pinned source revisions for:

- PCSX-ReARMed PS3 source
- RetroArch
- RetroArch menu assets
- RetroArch core metadata

The build script is `scripts/build_online_pkg.sh`. It builds the PS1 core, builds the Salamander EBOOT, links the real core into RetroArch, checks the SELF/SFO identities, packages the PKG, and writes a SHA-256 file plus a build manifest.

Expected CI artifacts:

- `Crash-Bash-Online-PS3.pkg`
- `Crash-Bash-Online-PS3.pkg.sha256`
- `BUILD-MANIFEST.txt`

Host protocol tests and web-lobby security tests are separate checks. A successful CI build proves that the package was produced and passed build-time checks; it does **not** prove that two physical PS3 consoles have completed an online match. RPCS3 and real-console testing remain necessary.

## Separate prototype components

The repository also contains an earlier native Tiny3D menu, a guest-room API backed by Vercel Blob, a JSON parser, a UDP input-packet codec, a UDP peer primitive, and PS1 disc/DAT extraction tools.

Those components are development prototypes. The current installable RetroArch PKG does **not** use the Vercel room API or the custom UDP peer code for gameplay. Actual match synchronization in this package is provided by RetroArch Netplay. The web room service and custom transport must not be described as a completed game-session path.

## Licensing and project scope

No copyrighted Crash Bash game files, BIOS files or disc dumps are committed. The build includes the relevant upstream license files for RetroArch (GPL-3.0), PCSX-ReARMed (GPL-2.0) and RetroArch assets (CC BY 4.0).

This is an unofficial homebrew project and is not affiliated with Sony, Activision or the original Crash Bash developers.
