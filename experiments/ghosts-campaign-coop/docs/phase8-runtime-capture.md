# Phase 8 — runtime evidence pipeline

## Objective

Phase 8 prepares the first reproducible evidence loop for the exact BLUS31270 TU 1.16 Campaign and Multiplayer executables. It does not claim that Campaign can create a second local client. The gate remains: prove the native local-client lifecycle and controller-to-client mapping before enabling any Campaign hook.

## 1. PS3MAPI runtime dump and ELF reconstruction

Added under tools/ps3mapi:

- dump_campaign_tu116.bat — manual, read-only Campaign PT_LOAD capture.
- dump_multiplayer_tu116.bat — manual, read-only Multiplayer PT_LOAD capture.
- SELF_MEMORY_MAP_TU116.json — exact segment sizes, virtual addresses, entry points.
- verify_tu116_dump.py — verifies exact SELF program headers, mode-specific files, 64 KiB padding, and executable-segment sanity before reconstruction.
- reconstruct_elf_from_ps3mapi_dumps.py — rebuilds a minimal ELF64 BE PPU analysis image from the SELF headers and process dumps.
- scan_runtime_strings.py — finds client/split-screen/controller/camera/HUD-related ASCII strings in the reconstructed ELF.
- compare_runtime_strings.py — compares string leads between Campaign and Multiplayer.
- diff_runtime_elfs.py — compares overlapping file-backed PT_LOAD bytes at matching virtual addresses, with block hashes and a clear limit that changed blocks are not function boundaries.

### Capture identity

Campaign and Multiplayer share Title ID BLUS31270. Therefore the two dump scripts are manual and must not be installed as an automatic wm_ingame/BLUS31270.bat. The operator must be inside the matching mode when running its script. webMAN's current-game PID is used by omitting proc; dump sizes are byte counts and the writer rounds reads to 64 KiB chunks.

The Campaign and Multiplayer segment outputs have distinct filenames. The verifier does not require manual renaming and rejects wrong profile metadata, incomplete chunks, empty executable segments, and unsafe output overwrites.

The reconstructed ELF is for local static analysis only. It preserves PT_LOAD program headers and the entry point, omits section headers, represents BSS via p_memsz, and may contain loader relocations. It is not a runnable replacement EBOOT.

## 2. Read-only pad enumeration probe v4

Added as a separate PS3 PRX under client/ps3/probes/pad_probe:

- Reads cellPadGetInfo2 only.
- Does not call cellPadInit, cellPadEnd, cellPadGetData, or pad-setting APIs.
- Does not write to game memory, install hooks, or change dvars.
- Requires an exact CAMPAIGN or MULTIPLAYER mode file; otherwise it fails closed and does not sample.
- Writes distinct Campaign and Multiplayer logs.
- Samples 30 times at one-second intervals, recording connected-port masks and per-port status/capability/device type.

The probe is built separately from the main bootstrap PRX. Its output is diagnostic only; it does not create a Local Client, Player Entity, camera, HUD, or input command stream.

The log comparator was strengthened: it now requires port 0 and port 1 to appear together in the same sample (mask 0x03) before reporting simultaneous two-pad visibility. Seeing port 0 in one sample and port 1 in another is not counted as simultaneous.

## 3. CI and tests

The GitHub Actions PS3 toolchain job now prepares libfs_stub and liblv2_stub and builds the separate pad-probe PRX alongside the existing bootstrap. The expected pad-probe artifact contains the PRX, fake-signed SPRX and ELF.

New host tests cover:
- exact Campaign and Multiplayer PT_LOAD profile verification;
- 64 KiB PS3MAPI dump sizes and rejection of truncated/zero-filled executable segments;
- refusal to overwrite SELF or existing output files;
- mode-specific dump filenames without manual renaming;
- simultaneous controller-port detection;
- runtime ELF binary-diff ranges and non-executable-segment handling;
- probe source safety (cellPadGetInfo2 only, no pad-data polling or memory writes).

Host and CI tests validate the tooling and build. They do not replace an actual PS3 load test.

## 4. Runtime steps still outstanding

The remaining evidence must come from the user's console:

1. Copy ghosts_campaign_pad_probe.fake.sprx to /dev_hdd0/tmp and place the exact CAMPAIGN mode file at /dev_hdd0/tmp/ghosts_probe_mode.txt.
2. While inside Campaign, request /ps3mapi.ps3?PROCESS+GETCURRENTPID, then load the PRX with /ps3mapi.ps3?MODULE+LOAD+PID+/dev_hdd0/tmp/ghosts_campaign_pad_probe.fake.sprx. Verify it via MODULE+GETALLPRXID and MODULE+GETNAME/GETFILENAME; MODULE LOAD may return HTTP 200 even if the load failed.
3. Wait 30 seconds, copy ghosts_campaign_pad_probe.log, then unload the module with MODULE+UNLOAD+PID+PRX_ID.
4. Repeat in Multiplayer using the MULTIPLAYER mode file and collect ghosts_multiplayer_pad_probe.log.
5. Run the comparator and share its JSON/log output.
4. Capture Campaign and Multiplayer PT_LOAD segments using their matching manual scripts.
5. Run verify_tu116_dump.py against the exact installed TU 1.16 SELF files.
6. Share the two verification JSON reports and string-diff report. Keep raw game dumps local unless a specific byte range is required for analysis.

## 5. Native player-two gate

A second local player is not proven by any of the following alone:
- port 1 is connected;
- a split-screen string exists;
- a GSC helper mentions player 2;
- a second entity is spawned;
- a PRX loads successfully.

Before a hook is considered, analysis must establish:
- two active local-client slots and their lifecycle;
- controller port 0/1 mapping to distinct command streams;
- distinct client/player-state/entity ownership;
- separate view/refdef/camera contexts;
- primary/secondary split-screen identity;
- Campaign startup and checkpoint/respawn behavior;
- Hesh remains an NPC and Logan remains player one.

**Phase 8 status:** the capture pipeline and read-only probe are prepared in source and CI. No PS3 hardware logs or TU 1.16 runtime dumps have been received in this phase, so no native player-two hook or playable Campaign co-op build is claimed.


## 6. CI artifact package

The PS3DK build job packages the diagnostic PRX, fake-signed SPRX, analysis ELF, Arabic usage notes, and both mode-selection text files as the ghosts-campaign-pad-probe artifact. The PRX passed prx-gen build/check in CI. This proves the module compiles and its imports are structurally valid; it does not prove that it has been loaded on a PS3.


## 7. Verified CI build result

GitHub Actions run 45019924511 passed all five jobs. The server/network suite completed 117/117 tests, and the PS3DK job built the pad-probe PRX and passed prx-gen validation. Artifact ID: 14753341454; package size: 100,676 bytes; SHA-256: 3cfc7716b9e9713e49b8006feac682f28f855d8b8cf7b70747786061f9dd2502.

The archive contains:
- ghosts_campaign_pad_probe.prx — 145,272 bytes;
- ghosts_campaign_pad_probe.fake.sprx — 147,192 bytes;
- ghosts_campaign_pad_probe.elf — 111,304 bytes;
- Arabic instructions and both Campaign/Multiplayer mode files.

The build is verified; hardware loading is not. Do not treat the CI artifact as a playable game mod.
