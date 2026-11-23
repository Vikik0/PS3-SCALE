# Crash Bash Online — PS3 hardware validation plan

## Why this test is required

The CI workflow proves that the PS3 binaries and PKG can be built. It does not prove that PCSX-ReARMed runs Crash Bash at full speed on a PS3 or that two consoles can keep a session synchronized.

The pinned PCSX-ReARMed core reports a `0x440000`-byte serialization state. The package enables RetroArch Netplay Lockstep so it keeps one shared state instead of a rollback state for every buffered frame. This reduces memory use, but every frame waits for player input and can expose network latency as input delay or stalls.

RetroArch's current Netplay FAQ cautions that PSX netplay is generally unsuitable because of processing cost. The tests below determine whether this PS3-specific build performs acceptably despite that warning.

## Required setup

- Two jailbroken PS3 consoles on the same compatible firmware family.
- The exact same `Crash-Bash-Online-Unified-PS3.pkg` installed on both consoles.
- A legal BIN/CUE dump of the same Crash Bash disc revision on both consoles.
- The exact same CUE file and byte-identical contents for every referenced BIN track.
- The same BIOS file on both consoles, or both consoles using the same HLE mode. A BIOS dumped from the user's own PS1 is recommended.
- A stable wired network for the first test; internet access for the relay test.
- At least two controllers; four controllers for the final multiplayer test.

Do not change the core version, CUE/BIN content, region, BIOS file (or HLE choice), Multitap option or synchronization mode between consoles during a test. The package stores its settings and logs under `/dev_hdd0/home/<user-id>/retroarch/CBON00001/`.

## Test 0 — Native lobby handoff

1. Install the single unified `Crash-Bash-Online-Unified-PS3.pkg` on both compatible PS3 consoles.
2. Launch **Crash Bash Online** from the XMB on the first console and create a public or private room from its built-in lobby.
3. Join the room from the second console using the six-character invite code.
4. Mark both players Ready. Confirm the room becomes launch-ready with two players; a four-seat room is not required.
5. Press START in the room on both consoles and confirm the CUE picker appears.
6. On the host console, select the Crash Bash CUE and press CROSS. Confirm the API locks the ready room before RetroArch starts hosting and publishes its private relay session.
7. On each console, select the intended CUE and press CROSS to confirm. Confirm the host waits for all active players and the same disc fingerprint is reported before it starts RetroArch.
8. Deliberately select a different CUE on one client. Confirm the room reports a disc mismatch and does not launch either console. Correct the selection and confirm the host proceeds only after the fingerprints match.
9. After confirming a CUE, change the selected file with UP/DOWN, rescan with SQUARE, or back out with CIRCLE. Confirm the previous fingerprint is cleared and the host keeps waiting until that player confirms again.
10. Toggle READY off and on after confirming a CUE. Confirm the disc confirmation is cleared and must be repeated.
11. On the client console, select the same CUE and confirm the companion finds the host through the RetroArch public lobby and starts RetroArch with `--connect` and `--mitm-session`, without manual Host List selection.
12. From the companion's home menu, also test **OPEN CRASH BASH ONLINE**.
13. For a controlled fault test, temporarily rename `USRDIR/retroarch` on one test console and confirm the lobby reports the missing embedded runtime rather than crashing; restore the folder after the test.
14. After the host starts, return the client to the room screen and confirm it shows **MATCH STARTED**, not the pre-match READY prompt. Press START, choose the same CUE and retry the automatic join flow.
15. Press CIRCLE during relay discovery to return to the room; verify the client can reopen the CUE picker without changing the already-started room lock.

Pass criteria: both players see the same room membership and Ready state; only the host can lock the room for launch; late joins are rejected; the client resolves the exact host entry, receives the relay endpoint only through its authenticated room token, and automatically joins. Started seats remain reserved for the match window. The missing-package path is handled cleanly. The Vercel companion does not carry game frames; actual synchronization is provided by RetroArch Netplay.

## Test 1 — Single-console baseline

1. Launch the single Crash Bash Online icon from the XMB.
2. Choose **OPEN CRASH BASH ONLINE** in the built-in lobby to start the local game using the selected CUE.
3. Start a local game and reach a playable arena.
4. Confirm video, audio, controller response and all four local controller slots.
5. Play for at least 10 minutes, including a busy arena with multiple effects.

Pass criteria: no crash, freeze, audio break-up, severe slowdown or missing controller input.

If the game fails locally, stop here. Netplay testing cannot be meaningful until the local PS1 emulation path is stable.

## Test 2 — Two consoles on the same LAN

1. Connect both PS3s to the same router, preferably by Ethernet.
2. Load the exact same CUE file and byte-identical referenced BIN tracks on both consoles.
3. On the host, start a Netplay session.
4. On the client, select the same CUE and press CROSS once. Confirm the companion resolves the host and launches RetroArch with the correct direct or relay endpoint.
5. Start a two-player Crash Bash match and play for at least 15 minutes.
6. Repeat once with the other console hosting.

Pass criteria: both players' inputs are registered, the game does not desynchronize, and there are no persistent freezes or disconnects.

Record whether input delay is acceptable. Lockstep is expected to wait for the slowest player's input; some delay is therefore possible.

## Test 3 — Two consoles over the internet

1. Keep the same package, exact CUE/BIN content, BIOS file (or HLE choice) and core settings.
2. Put the consoles on different internet connections.
3. Leave the package's public lobby, NAT traversal and Madrid relay defaults enabled.
4. On the host, start Netplay. On the client, select the same CUE and press CROSS once; wait for automatic relay discovery and connection.
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
4. Disconnect one waiting player or stop its heartbeat for more than 90 seconds but less than 10 minutes. Confirm the seat remains visible, but READY and the disc confirmation expire. Send a heartbeat with the old cached READY value and confirm it does not restore READY; the player must explicitly press READY and confirm the disc again.
5. Remove one player from a three-player room. Confirm the remaining players lose READY and their previous disc confirmations before another launch.
6. Close RetroArch normally and relaunch it to confirm settings persist.

Pass criteria: password protection behaves as expected, stale players cannot remain READY, membership changes invalidate old disc confirmations, reconnection does not corrupt the session, and the saved nickname/network settings remain available.

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
