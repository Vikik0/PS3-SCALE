# Crash Bash Online — PS3 build workflow

The workflow `.github/workflows/crash-bash-build.yml` runs on code/build changes
to `crash-bash-enhanced`, matching pull requests, and manual dispatch.
Documentation-only changes do not trigger the expensive PS3 cross-build.

## Build scope

The installable PKG is built from:

- RetroArch's PSL1GHT PS3 frontend with Netplay enabled.
- A statically linked PCSX-ReARMed PS1 core.
- Local single-player launch passes the embedded Crash Bash CUE as the content argument; it does not use RetroArch `--menu`, which would open the menu without loading the game.
- Port 1 Multitap enabled in the core.
- RetroArch Netplay lockstep enabled to reduce the memory cost of the
  0x440000-byte PS1 savestate.
- Public lobby announcement, NAT traversal and the Madrid relay endpoint.
- An app-specific PS3 user profile under `retroarch/CBON00001`.

The earlier Tiny3D shell, Vercel room API and custom UDP input codec are separate
prototypes; they are not the gameplay path in this PKG.

## CI checks

1. Bash syntax check for the PS3 package build script.
2. Host-side protocol, JSON, archive, netcode, UDP peer, disc-browser, fingerprint and game-launch tests.
3. Both BIN/CUE staging suites, including missing-track, sector-alignment, path-traversal, duplicate-name and unexpected-file rejection cases.
4. Web lobby token/security tests.
5. PS3 cross-build, SELF/SFO identity checks, PKG header validation and
   SHA-256 verification.

The PS3 runtime is pinned to RetroArch's RGUI menu, so the package includes only shared fonts and RGUI assets; unused XMB, Ozone and wallpaper libraries are excluded. PSL1GHT's package tool has a 1,024-entry cap, and the Makefile now lists the unfinalized PKG and verifies every staged path and the item count before NPDRM finalization.

The PS3 workflow builds in two stages: `Crash-Bash-RetroArch-Stage` contains the verified embedded runtime, and `Crash-Bash-Online-Unified-PS3-PKG` contains the single installable lobby + RetroArch PKG, checksum and manifest. The latter deliberately excludes the user's game image and BIOS.

A green build is not a hardware certification. RetroArch's upstream Netplay FAQ
warns that PSX netplay can be too demanding; lockstep is a mitigation, and
two-console testing on real PS3 hardware is still required. Four-player support
must not be advertised as verified until the four-player test passes.

## Private game-inclusive package

The separate workflow `.github/workflows/crash-bash-personal-build.yml` is stored on `main`. It embeds the user's private BIN/CUE dump and encrypts the resulting one-PKG output to the recipient certificate. See [PERSONAL_BUILD_GUIDE.md](PERSONAL_BUILD_GUIDE.md) for the secure Dropbox handoff, runtime run ID, decryption and hardware limitations.

The currently verified runtime-stage run is `45856871351`. The personal workflow checks the source run's branch, workflow path, success conclusion and unexpired `Crash-Bash-RetroArch-Stage` artifact before it downloads the runtime.

The GitHub Actions secret `CRASH_BASH_DISC_ARCHIVE_URL` must be populated by the repository owner immediately before dispatch with a single-use HTTPS Dropbox download URL. The public repository must never contain the BIN/CUE archive, BIOS, Dropbox URL or private decryption key.
