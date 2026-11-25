# Crash Bash Online — hardware test record

Use one copy of this record per test session. Do not include PSN IDs, passwords, private IP addresses, console account data, game dumps or BIOS files in shared reports.

## Build under test

- Date/time:
- Tester:
- Package version / commit:
- PKG SHA-256:
- Package type: [ ] unified external-disc PKG  [ ] private game-inclusive PKG
- PS3 application ID: `CBON00001`
- RetroArch version / build:
- PCSX-ReARMed core build:
- Core options:
- Disc region:
- CUE SHA-256:
- BIN SHA-256:
- BIOS mode: [ ] HLE on all consoles  [ ] same user-dumped BIOS on all consoles

## Console and network setup

| Field | Console A (host) | Console B (client) | Console C | Console D |
|---|---|---|---|---|
| Model / revision | | | | |
| Firmware family | | | | |
| CFW/HEN version | | | | |
| Ethernet/Wi-Fi | | | | |
| Controller count | | | | |
| Free storage before install | | | | |

Network topology: [ ] same LAN  [ ] separate internet connections

Connection type: [ ] wired  [ ] wireless

Router/NAT notes (no public IPs or credentials):

## Test results

| Test | Pass / Fail / Not run | Duration | Input delay / stalls | Disconnects / desyncs | Notes |
|---|---|---:|---|---|---|
| 0 — Lobby handoff and disc confirmation | | | | | |
| 1 — Single-console local baseline | | | | | |
| 2 — Two consoles on same LAN | | | | | |
| 3 — Two consoles over internet relay | | | | | |
| 4 — Four-player Multitap | | | | | |
| 5 — Private room and recovery | | | | | |

## Detailed observations

- Exact step where a failure occurred:
- Screen/status text:
- Did both consoles use byte-identical CUE/BIN content?
- Did both consoles use the same BIOS/HLE mode and core options?
- Was the host visible in the RetroArch lobby?
- Did the client receive a relay endpoint?
- Were all controller slots independent?
- Did the room recover after a client reconnect?

## Evidence and logs

- Host RetroArch log location:
- Client RetroArch log location:
- Lobby API time window (UTC):
- Optional screenshots/video:
- Relevant SHA-256 values for logs:

Remove account names, credentials, private addresses and any personal data before sharing evidence.

## Release decision

- [ ] Local PS1 emulation stable
- [ ] Two-console LAN session completed
- [ ] Two-console internet relay session completed
- [ ] Four-player Multitap completed
- [ ] No unresolved crash, freeze or desynchronization
- [ ] Evidence reviewed

Do not mark online play as hardware-verified until Tests 2 and 3 pass. Do not advertise four-player online support until Test 4 passes.
