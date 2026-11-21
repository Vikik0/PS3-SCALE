# Crash Bash Enhanced VSH loader

`crash_bash_loader.fake.sprx` is a small VSH-side helper for Cobra/Mamba firmware with PS3MAPI enabled.

## What it does

- Polls the PS3MAPI process list from VSH.
- Detects `ps1_emu`, `ps1_netemu`, and `ps1_newemu`.
- Loads the process-local `crash_bash_input.fake.sprx` into the detected emulator process.
- Does not inject into ordinary PS3 game processes.
- Avoids repeated injection attempts for the same process ID.

The input bridge is expected at:

`/dev_hdd0/game/CBAS00001/USRDIR/crash_bash_input.fake.sprx`

## Loading

The loader itself must be loaded into VSH by a Cobra-compatible VSH plugin loader, such as the PS3MAPI load-module operation. It can also be placed in the CFW/HEN boot plugin list if the installed environment supports that mechanism.

This helper requires the Cobra/Mamba PS3MAPI syscall8 interface (`0x7777`). On firmware without that interface it will remain idle and will not inject anything.

## Limitations

- The loader has not been tested on a physical PS3 yet.
- It detects PS1 emulator process names, not the Crash Bash disc serial. The input bridge remains inactive until a user opens its in-game menu, but the menu hook is present in any PS1 emulator process into which it is injected.
- The input bridge now uses TLS 1.2 and certificate-verified WSS for the relay, with the system CA bundle embedded at build time.
- The PS3 transport has host integration coverage against a local HTTPS/WSS server, but has not been tested on a physical PS3.
- Public Worker deployment remains blocked until the Cloudflare Actions credentials are configured.
- The current build produces PRX/Sprx modules; an installable PKG wrapper is still outstanding.
