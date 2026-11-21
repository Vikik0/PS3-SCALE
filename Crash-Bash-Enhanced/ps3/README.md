# PS3 input bridge prototype

This is the first native PS3-side component for Crash Bash Enhanced.

## What it does

- Hooks the PS3 `cellPadGetData` and `cellPadGetInfo2` function descriptors in the process where the module is loaded.
- Leaves the physical controller on PS3 port 0 as the local player.
- Provides three remote PS1 controller slots when the bridge is explicitly activated.
- Uses the PS3 libpad report layout (digital buttons plus analog axes).
- Starts inactive and restores the original function descriptors when unloaded.

## What it does not do yet

- It does not connect to the lobby service.
- It does not send or receive UDP packets.
- It does not synchronize game frames.
- It has not been verified against the PS3 PS1 emulator or Crash Bash on physical hardware.

The online architecture is based on the upstream Crash Bash source port's existing four-player deterministic UDP lockstep implementation. The upstream code is pinned as a Git submodule at `../upstream/crash_bash_decomp`; see its `port/platform/net.c` and `docs/PORT.md`.

## Target process

This module is intended for the PS3 PS1 emulator process that runs the Crash Bash title. It is not a VSH boot plugin and must not be loaded into unrelated games or system processes.

The exact emulator process, multitap configuration, and controller-port mapping must be confirmed on the target console before enabling the bridge. The module currently leaves the bridge inactive on load to avoid changing normal controller behavior.

## Build

```bash
make PS3DEV=/path/to/ps3dev PS3DK=/path/to/ps3dev/ps3dk
```

The CI build uses the pinned PS3DK PPU toolchain and produces an unsigned PRX plus a fake-signed SPRX for CFW development. The fake-signed artifact is not a retail-signed package.

## Safety and compatibility

- The module patches only the current process's imported function descriptors.
- Both descriptors are restored on module unload.
- The network layer must validate all packets before calling `cb_input_set_remote_state`.
- The first target is the NTSC-U Crash Bash executable supported by the upstream decompilation project. Other regions need separate validation.
