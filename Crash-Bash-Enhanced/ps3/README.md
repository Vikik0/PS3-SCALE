# PS3 input bridge and netplay adapter

This folder builds two PS3 PRXs and an installable homebrew PKG:

- `crash_bash_input.fake.sprx` runs inside the PS1 emulator and hooks the controller API.
- `crash_bash_loader.fake.sprx` runs as a VSH boot plugin and injects the input bridge into a detected PS1 emulator process.
- `crash_bash_online_installer.gnpdrm.pkg` installs the launcher, both PRXs, and the boot-plugin registration helper.

The package contains no Crash Bash ISO, ROM, BIOS, music, textures, or other proprietary game data. Use a legally obtained game copy.

## Implemented in the current prototype

- Hooks the process-local `cellPadGetData`, `cellPadGetDataExtra`, and `cellPadGetInfo2` function descriptors.
- Builds a VSH-side loader that detects `ps1_emu`, `ps1_netemu`, and `ps1_newemu` processes and injects the input bridge through Cobra/Mamba PS3MAPI.
- Keeps the physical DualShock controller as the local input source and exposes four standard PS1 controller ports when online input is activated.
- Integrates the upstream Crash Bash four-player deterministic UDP lockstep implementation from the pinned `../upstream/crash_bash_decomp` submodule.
- Adapts the upstream timing and file calls to PS3 APIs.
- Provides a relay-backed room flow: create or join a Worker room, receive roster updates, and start from the host's `START` control.
- Intercepts the upstream lockstep `sendto`, `recvfrom`, and `select` calls in relay mode. It maps authenticated sender IDs to deterministic virtual addresses, routes host records to the correct peer, coalesces superseded retransmissions, and flushes CBU1 batches on a 220 ms cadence. A 12-step input delay is currently used to absorb the batching interval.
- Uses certificate-verified TLS 1.2 for HTTPS/WSS, including SNI and an embedded trusted-CA bundle. The PS3 clock must be set correctly for certificate validation.
- Computes a guarded FNV-1a fingerprint over the documented `ps1_netemu` 4.86 RAM window (base `0x770780`, size 2 MiB). A page-attribute probe disables the fingerprint if the range is not readable.

## Install and open the menu

1. Install `crash_bash_online_installer.gnpdrm.pkg` from the PS3 Package Manager on a Cobra-compatible CFW/HEN setup.
2. Open **Crash Bash Online** from the XMB.
3. Confirm the prompt to add the VSH loader to `/dev_hdd0/boot_plugins.txt`.
4. Reboot the PS3 so the VSH loader starts.
5. Start your legally obtained supported Crash Bash copy in the PS1 emulator.
6. Press **SELECT + L3 + R3** to open the in-game room menu. Enter the Worker hostname without `https://` and use port `443` unless your deployment specifies another port.

The installer adds this loader path:
`/dev_hdd0/game/CBAS00001/USRDIR/crash_bash_loader.fake.sprx`

To disable the loader, remove that exact line from `/dev_hdd0/boot_plugins.txt` and reboot. The game files and plugin payload remain installed until the title is deleted from the XMB.

## Current release limits

- CI cross-compiles both PRXs, validates their PRX structures, builds the installer SELF, and produces both `.pkg` and finalized `.gnpdrm.pkg` outputs. This proves build/package generation, not successful installation or operation on physical hardware.
- The in-game menu, process injection, controller mapping, and relay gameplay still require physical PS3 testing. The loader currently detects PS1 emulator process names rather than the Crash Bash disc serial, so it may inject the inactive bridge into another PS1 emulator session.
- The RAM fingerprint offset is documented for `ps1_netemu` 4.86; the page probe does not prove the same offset on every firmware or emulator revision. Firmware 4.93 and other revisions remain unverified.
- The relay transport and lockstep adapter have host-side integration tests, but latency, packet loss, disconnect/rejoin, and long-session behavior need real-network testing.
- Memory-card save synchronization is not implemented. The first intended test target is same-build Battle/minigame sessions, not Adventure progression.
- The website currently provides guest sessions and room matchmaking, not a browser game runtime. Browser-to-PS3 gameplay is therefore not available yet.

## Build

```bash
make PS3DEV=/path/to/ps3dev PS3DK=/path/to/ps3dev/ps3dk
```

GitHub Actions downloads the pinned PS3DK PPU toolchain, builds the runtime and import libraries, compiles both PRXs, validates their import tables, and creates the installable PKG. The upstream implementation and its license remain in the pinned submodule; this adapter does not copy or replace that source.

## Network behavior

The direct-UDP API remains best-effort and uses STUN-assisted hole punching. Relay mode sends the upstream CBN1 lockstep datagrams through the room-scoped WebSocket relay as CBU1 batches; the Worker routes them by authenticated session ID and wraps received datagrams in CBR1 sender envelopes. The Worker receives opaque gameplay packets in relay mode. PSN authentication is not used.