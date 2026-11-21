# PS3 input bridge and netplay adapter

This folder builds the first native PS3 PRX for Crash Bash Enhanced.

## Implemented in the current prototype

- Hooks the process-local `cellPadGetData`, `cellPadGetDataExtra`, and `cellPadGetInfo2` function descriptors.
- Keeps the physical DualShock controller as the local input source.
- Exposes four standard PS1 controller ports to the PS1 emulator when online input is activated.
- Integrates the upstream Crash Bash four-player deterministic UDP lockstep implementation from the pinned `../upstream/crash_bash_decomp` submodule.
- Adapts the upstream SDL timing/file calls to PS3 APIs.
- The legacy direct-UDP path uses STUN Binding requests on the game socket to discover the external IPv4/port and detect some symmetric-NAT mappings. Relay-backed rooms bypass STUN and use authenticated virtual peer addresses.
- Provides direct-UDP and relay-backed host/join/lobby APIs through `net_adapter.h`. Relay mode creates or joins a Worker room, receives room roster updates, and uses the host-only `START` control before the upstream lobby begins.
- The PS3 adapter now intercepts the upstream lockstep `sendto`, `recvfrom`, and `select` calls in relay mode. It maps authenticated sender IDs to deterministic virtual addresses, routes host records to the correct peer, coalesces superseded retransmissions, and flushes CBU1 batches on a 220 ms cadence to stay below the Worker message cap. The relay uses a 12-step input delay to absorb batching latency. Host-side tests cover guest/host tickets, room roster mapping, START, CBU1 batches, and CBR1 packet decoding.
- The PS3 HTTP and WebSocket clients currently use plain HTTP and `ws://`. TLS/HTTPS and WSS are not implemented. Cloudflare Workers normally require secure transport, so this build is not yet ready to connect to a public `workers.dev` deployment.
- Computes a guarded FNV-1a fingerprint over the PS1 RAM window documented for `ps1_netemu` 4.86 (base `0x770780`, size 2 MiB). A page-attribute probe disables the fingerprint if the address range is not readable.

## Still incomplete / not verified

- There is no user-facing PS3 host/join menu or on-screen keyboard yet. The native relay APIs are exposed for a future menu/configuration layer; they are not yet invoked by a user-facing in-game flow.
- The relay shim is now connected to the upstream lockstep socket calls, but it has only host-side integration coverage. It has not been verified on physical PS3 hardware, and public Worker access remains blocked by the missing TLS/WSS implementation and Cloudflare deployment credentials.
- No TURN server is included. Relay mode is intended to avoid direct UDP/NAT traversal; packet loss, latency, reconnect behavior, and long-session stability still need real-network testing.
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

The direct-UDP API still uses STUN-assisted hole punching and remains best-effort. Relay mode sends the upstream CBN1 lockstep datagrams through the room-scoped WebSocket relay as CBU1 batches; the Worker routes them by authenticated session ID and wraps each received datagram in a CBR1 sender envelope. The Worker therefore does receive opaque gameplay packets in relay mode.

The upstream implementation and its license remain in the pinned submodule at `../upstream/crash_bash_decomp`. The PS3 adapter does not copy or replace that upstream source.
