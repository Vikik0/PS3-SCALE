# Crash Bash Online — PS3 hardware validation plan

## Why this test is required

The CI workflow proves that the PS3 binaries and PKG can be built. It does not prove that PCSX-ReARMed runs Crash Bash at full speed on a PS3 or that two consoles can keep a session synchronized.

The pinned PCSX-ReARMed core reports a `0x440000`-byte serialization state. The package enables RetroArch Netplay Lockstep so it keeps one shared state instead of a rollback state for every buffered frame. This reduces memory use, but every frame waits for player input and can expose network latency as input delay or stalls.

RetroArch's current Netplay FAQ cautions that PSX netplay is generally unsuitable because of processing cost. The tests below determine whether this PS3-specific build performs acceptably despite that warning.

## Required setup

- Two jailbroken PS3 consoles on the same compatible firmware family.
- The exact same `Crash-Bash-Online-PS3.pkg` installed on both consoles.
- A legal BIN/CUE dump of the same Crash Bash disc revision on both consoles.
- The exact same CUE file and byte-identical contents for every referenced BIN track.
- The same BIOS file on both consoles, or both consoles using the same HLE mode. A BIOS dumped from the user's own PS1 is recommended.
- A stable wired network for the first test; internet access for the relay test.
- At least two controllers; four controllers for the final multiplayer test.

Do not change the core version, CUE/BIN content, region, BIOS file (or HLE choice), Multitap option or synchronization mode between consoles during a test. The package stores its settings and logs under `/dev_hdd0/home/<user-id>/retroarch/CBON00001/`.

## Test 0 — Native lobby handoff

1. Install both the RetroArch game package (`CBON00001`) and the native lobby companion (`CBEN00001`) on two compatible PS3 consoles.
2. Launch the native lobby companion on the first console and create a public or private room.
3. Join the room from the second console using the six-character invite code.
4. Mark both players Ready. Confirm the room becomes launch-ready with two players; a four-seat room is not required.
5. Press START in the room on either console. Confirm it opens the installed Crash Bash Online RetroArch app.
6. From the companion's home menu, also test **OPEN CRASH BASH ONLINE**.
7. Temporarily test without the RetroArch package installed and confirm the launcher reports the missing app rather than crashing.

Pass criteria: both players see the same room membership and Ready state, the CUE picker finds the local disc, the room host starts RetroArch with Netplay hosting enabled, the client opens the RetroArch menu, and the missing-package path is handled cleanly.

The host's Netplay nickname is `CB-ROOMCODE-HOST`. On the client, open **Netplay Host List** and select that exact nickname. Client selection remains manual; the lobby does not pass a transport endpoint or room ID into Netplay and does not synchronize game state itself.

## Test 1 — Single-console baseline

1. Launch Crash Bash Online from the XMB.
2. Load the game's CUE file.
3. Start a local game and reach a playable arena.
4. Confirm video, audio, controller response and all four local controller slots.
5. Play for at least 10 minutes, including a busy arena with multiple effects.

Pass criteria: no crash, freeze, audio break-up, severe slowdown or missing controller input.

If the game fails locally, stop here. Netplay testing cannot be meaningful until the local PS1 emulation path is stable.

## Test 2 — Two consoles on the same LAN

1. Connect both PS3s to the same router, preferably by Ethernet.
2. Load the exact same CUE file and byte-identical referenced BIN tracks on both consoles.
3. On the host, start a Netplay session.
4. On the client, refresh the Netplay Host List and join the host.
5. Start a two-player Crash Bash match and play for at least 15 minutes.
6. Repeat once with the other console hosting.

Pass criteria: both players' inputs are registered, the game does not desynchronize, and there are no persistent freezes or disconnects.

Record whether input delay is acceptable. Lockstep is expected to wait for the slowest player's input; some delay is therefore possible.

## Test 3 — Two consoles over the internet

1. Keep the same package, exact CUE/BIN content, BIOS file (or HLE choice) and core settings.
2. Put the consoles on different internet connections.
3. Leave the package's public lobby, NAT traversal and Madrid relay defaults enabled.
4. Host from one console and join from the other through the host list.
5. Play for at least 15 minutes.
6. If the room is visible but the connection fails, repeat with a host-side TCP port-forward for port `55435`.

Pass criteria: the client can join, the match remains synchronized, and the session does not disconnect during the test.

## Test 4 — Four-player Multitap

1. Use four controllers across the available consoles. A practical first setup is two PS3s with two controllers attached to each; four separate consoles can also be used.
2. Host from one console and join from the other. Use Netplay's device-assignment controls to map each local controller to a distinct Crash Bash player slot.
3. Confirm that all four player slots are assigned and respond independently.
4. Complete at least one full match.

Pass criteria: all four players can control their own Crash Bash character without one player's input being duplicated into another slot.

## Test 5 — Private room and recovery

1. Set a Netplay Server Password on the host.
2. Confirm that an uninvited client cannot join without the password.
3. Confirm that an invited client can join with the password.
4. Disconnect one client intentionally, then attempt to rejoin.
5. Close RetroArch normally and relaunch it to confirm settings persist.

Pass criteria: password protection behaves as expected, reconnection does not corrupt the session, and the saved nickname/network settings remain available.

## Logs and evidence

After each failed test, preserve the RetroArch log from the app-specific profile's `logs` folder under `/dev_hdd0/home/<user-id>/retroarch/CBON00001/` on both consoles. Record:

- Which console was host and which was client.
- Whether the consoles were on the same LAN or different internet connections.
- Whether the room appeared in the host list.
- The exact stage where connection or gameplay failed.
- Whether the failure was a crash, freeze, slowdown, desync, input delay or disconnect.

Do not publish game dumps, BIOS files, console account data or private network credentials with the logs.

## Release gate

Do not describe online play as confirmed until Tests 2 and 3 pass on physical PS3 hardware. Do not advertise four-player online support until Test 4 passes. A green GitHub Actions build alone is not a release gate for multiplayer functionality.
