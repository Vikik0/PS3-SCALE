# PS3 input bridge and netplay adapter

This folder builds the first native PS3 PRX for Crash Bash Enhanced.

## Implemented in the current prototype

- Hooks the process-local `cellPadGetData`, `cellPadGetDataExtra`, and `cellPadGetInfo2` function descriptors.
- Keeps the physical DualShock controller as the local input source.
- Exposes four standard PS1 controller ports to the PS1 emulator when online input is activated.
- Integrates the upstream Crash Bash four-player deterministic UDP lockstep implementation from the pinned `../upstream/crash_bash_decomp` submodule.
- Adapts the upstream SDL timing/file calls to PS3 APIs.
- Uses STUN Binding requests on the same UDP socket as the game transport to discover the external IPv4/port. A second STUN destination is used to detect some symmetric-NAT mappings.
- Provides native host/join/lobby functions through `net_adapter.h`; the game-input hook polls the lobby and feeds synchronized pad records into the emulator.
- Includes an RFC 6455 WebSocket client and a PS3 relay client that requests a one-time guest ticket, joins the room transport, sends CBU1 batches with per-peer routing, includes a six-frame batching queue, and decodes authenticated CBR1 sender envelopes. Host-side tests verify that six frame datagrams are sent as one relay message. The scheduler is not yet wired into upstream `sendto`/`recvfrom`.
- The relay client currently uses plain HTTP and `ws://` for the prototype. TLS/WSS is not implemented, so do not treat this as production-secure.
- Computes a guarded FNV-1a fingerprint over the PS1 RAM window documented for `ps1_netemu` 4.86 (base `0x770780`, size 2 MiB). A page-attribute probe disables the fingerprint if the address range is not readable.

## Still incomplete / not verified

- There is no user-facing PS3 host/join menu yet. The native host/join APIs are compiled, but they are not yet connected to the web lobby or an on-screen keyboard/configuration flow.
- The relay client is not yet wired into the upstream UDP lockstep adapter or an in-game room-code screen. The PRX still starts the upstream direct-UDP lobby path, so cross-platform matches are not playable yet.
- No TURN server is included. The WebSocket relay is the intended replacement for direct UDP across browser/PS3, but the upstream lockstep transport still needs a relay shim, packet routing, and batching.
- Memory-card save synchronization is stubbed as empty data. The first target should be same-build Battle/minigame sessions, not Adventure progression.
- The PS1 RAM fingerprint uses a community-documented `ps1_netemu` 4.86 memory map. The page-attribute probe confirms that the range is mapped and readable, but it does not prove that the same offset is correct on every firmware/emulator revision. The exact 4.93 (or other target firmware) memory layout still needs hardware verification.
- The exact PS1 emulator process, multitap behavior, title-region identity and controller-port mapping have not been verified on physical PS3 hardware.

## Target process

The PRX is intended for the PS3 PS1-emulator process running the supported Crash Bash title. It is not a VSH boot plugin and must not be loaded into unrelated games or system processes.

The module installs the hooks but leaves virtual-pad injection inactive until the netplay layer starts. This avoids changing normal controller behavior when the plugin is merely loaded.

## Build

```bash
make PS3DEV=/path/to/ps3dev PS3DK=/path/to/ps3dev/ps3dk
```

GitHub Actions downloads the pinned PS3DK PPU toolchain, builds the runtime and import libraries, compiles the PRX, validates its import table, and uploads an unsigned PRX plus a fake-signed CFW development SPRX. A successful CI build proves compilation and PRX packaging only; it does not prove gameplay on hardware.

## Network behavior

The game-data channel is direct UDP using the upstream lockstep protocol. The matchmaking Worker is control-plane only and never receives gameplay packets. The initial connection strategy is STUN-assisted UDP hole punching; this is best-effort and not a universal NAT traversal solution.

The upstream implementation and its license remain in the pinned submodule at `../upstream/crash_bash_decomp`. The PS3 adapter does not copy or replace that upstream source.
