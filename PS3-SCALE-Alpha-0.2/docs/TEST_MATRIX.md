# Device test matrix — Alpha 0.33

Primary device (user-reported):
- Family: PS3 Slim
- System software: CFW 4.93
- Exact CECH retail model: not yet confirmed
- CEX / DEX: not yet verified on the console; the app queries this read-only when supported
- Cobra version: 8.5 (user-reported)
- Available HDD space: not yet confirmed

The About page reports the Platform ID, mapped hardware family/series, firmware, and console type. This is a read-only identification step; the user must still confirm the on-device values before a hardware-specific feature is enabled.

## Initial safety checks

1. Install only the PS3 SCALE manager PKG; do not apply an unreviewed game patch.
2. Launch the app and confirm it returns to XMB with Circle.
3. Run scan twice and confirm the library count is stable (no duplicates).
4. Verify PS2ISO lists `.ISO`, `.BIN`, `.IMG` and `.MDF` files and keeps `.BIN.ENC` in the PS2 Classics category.
5. Confirm folders, ISO files, PARAM.SFO files and EBOOT files remain unchanged.
6. Test with HDD only, then with one USB device.
7. Keep at least 1 GB free on the internal HDD.
8. Confirm the Home screen shows one spiral staircase and no FEATURES heading; confirm the About screen has no extra icon image.
9. Confirm Music Playlist values read Music 1 / Music 2 and that the storage preference defaults to TEMPORARY.

## Online DB and NCL installation

1. Open Online DB for the first time in an app session; confirm an HTTPS refresh starts automatically and cached results remain visible.
2. Press Triangle and confirm a real profile + community-index refresh starts. Start should remain a refresh shortcut.
3. Confirm Select opens search and Square changes the filter. The default Graphics + Any Region/Version filter should show candidate results without cycling Square twice.
4. Select a known local HDD game and confirm a community NCL result is installable only when Title ID + APP_VER match exactly.
5. Confirm Cross refuses a non-graphics category, a non-NCL file, a mismatched version, a game outside /dev_hdd0, or a missing Artemis installation.
6. Confirm Artemis detection works for both ARTPS3001 and ARTZ00001 package IDs.
7. Download one reviewed NCL file and verify it appears in the detected Artemis USERLIST directory with a PS3SCALE_TitleID_APPVER filename.
8. Confirm an existing file is never overwritten, a binary/ELF/PKG response is rejected, and a downloaded file with a byte size different from the index metadata is rejected.
9. In TEMPORARY mode, verify the downloaded file is removed on the next PS3 SCALE launch. In PERMANENT mode, verify a new file remains after restarting PS3 SCALE.
10. Open Artemis and verify the NCL entry is visible before applying it. Do not test an unreviewed code against a valuable save or online session.

## Real before/after previews

1. Open Game Details for a title without preview assets; confirm the UI shows NO MATCHED SCREENSHOTS rather than a concept illustration.
2. Add a genuine screenshot pair for one exact Title ID + APP_VER using the documented filenames.
3. Confirm both images appear together and that a missing half-pair does not display a one-sided comparison.
4. Confirm the preview for one game/version is not reused for another version.

No real screenshot pairs are currently bundled in the repository; this part requires genuine captures from the original and enhanced game builds.

## webMAN launch and profile safety

Host CI also runs the actual local HTTP transport against a loopback webMAN mock:
- A 200 response is accepted for a valid game-launch request.
- A 302 redirect is rejected rather than treated as proof of a launch.
- A PS3MAPI JSON body split across TCP packets is fully read and validated.
- A server that accepts a request but never responds is bounded by the configured timeout.

1. Test a normal unmodified game launch with webMAN MOD available and no profile armed.
2. Test a normal launch with PS3MAPI disabled; the game should still start unmodified.
3. Arm a one-write profile, then make PS3MAPI unavailable before Start Game. PS3 SCALE should remove its own pending script and still start the game unmodified.
4. Disable/unload webMAN and confirm the app reports the local HTTP/network error without freezing.
5. Keep webMAN active but move its plugin file outside the two common detection paths; Companion Tools should still attempt the local service instead of blocking the launch action.
6. Exit PS3 SCALE while a companion launch request is waiting; confirm shutdown completes cleanly after the bounded request finishes.
7. Test one harmless, reversible, single-write profile with a known expected byte at the exact address.
8. Confirm profile identity scans run on a background worker and D-pad / Circle input remains responsive while the HDD or mounted-disc check runs.
9. Change or unmount the selected game after arming; confirm no stale profile is applied and the original game is started unmodified when webMAN is available.
10. Disable PS3MAPI while keeping webMAN active; confirm CROSS/START starts the game unmodified and removes only the PS3 SCALE-owned script.
11. Disable webMAN; confirm the app shows a connection error and does not falsely report a successful launch.
12. Confirm a stale or edited PS3 SCALE script is blocked, while a foreign webMAN script is never overwritten or deleted.
13. Confirm that closing the game clears the process-memory change.

## PS2 CONFIG Manager — Alpha 0.31

1. Open Game Details for a PS2 ISO and press SQUARE; confirm PS2 CONFIG Manager opens.
2. Confirm the current CONFIG, backup and rollback states and file sizes match the files on disk.
3. Back up an existing `.CONFIG`; confirm the saved file is byte-identical to the original.
4. Attempt a second backup; confirm the existing backup is not overwritten.
5. Change the current CONFIG while retaining the old backup; confirm online replacement is blocked as stale.
6. Restore the saved backup; confirm the old current CONFIG is preserved as a rollback copy.
7. Press restore again; confirm it reports that the current file already matches the backup and does not rewrite it.
8. For a game with an exact database match and no existing CONFIG, download and confirm the file is created beside the ISO.
9. Repeat backup/restore for an installed PS2 Classics ENC only when the webMAN PS2CONFIG directory exists.
10. Confirm oversized files, invalid paths, leftover temporary files and rollback conflicts are rejected without modifying the ISO, EBOOT or installed title.
11. In host CI, create a symlink at a CONFIG source and at a temporary backup path; confirm both are rejected. On PS3, verify O_EXCL prevents an existing temporary path from being opened for writing and rename does not write through a linked target.
12. Pre-create a valid rollback snapshot, then install a byte-matching replacement; confirm the target is replaced atomically and the existing rollback and original backup remain unchanged.
13. Launch the same PS2 game with and without the CONFIG and record actual behavior; do not infer compatibility from a successful file install alone.

## PS2 Enhancer — Alpha 0.34

1. Probe root-level and nested ISO9660 paths with single- and multi-extent SYSTEM.CNF / BOOT files; verify ordered assembly and reject malformed, oversized, mismatched or non-sector-aligned extent chains.
2. Verify the local netemu options and game-patch Apply actions are enabled only after syscall detection confirms a PS3 Slim / Super Slim. Confirm FAT hardware (including CECHA/B and CECHC/E) and unknown / unavailable hardware show NETEMU HARDWARE REQUIRED and reject Cross without touching any CONFIG.
3. Verify PS2 ISO community CONFIG lookup uses a known hardware profile (NET for Slim / Super Slim, GX / SOFT for the mapped FAT families); unknown hardware must not guess a preferred ISO CONFIG type. PS2 Classics exact ENC lookup remains available.
4. Verify the Apply action is available only when every exact-match patch address maps to a file-backed executable PT_LOAD segment; one invalid address must make the entire profile unavailable.
5. On a confirmed Slim / Super Slim PS2 ISO, confirm L2H is selected by default and that the original ISO hash and size do not change after applying a profile.
6. Apply L2H only (`0x46`); launch the same title and record loading, frame pacing, audio and any rendering differences.
7. Enable sharper pixels (`0x44`) only; compare XMB smoothing On/Off separately and record aliasing/softness.
8. Enable conservative frame pacing (`0x48`, mode 1, delay 20,000) only; record stutter, audio and timing. If any regression occurs, restore the saved CONFIG.
9. Combine options only after the individual tests pass. Do not assume combined effects are additive.
10. Confirm the first apply automatically creates a byte-identical backup. If the active CONFIG differs from that saved backup, confirm the tool blocks another profile until the user restores the saved backup; it must never overwrite that backup.
11. Confirm an existing community CONFIG that contains commands outside the local allow-list is never replaced by a local profile. Confirm a stale backup, invalid CONFIG, oversized file, pre-existing temporary path or rollback conflict blocks the write without changing the current CONFIG.
12. Confirm the generated sidecar is named exactly `<game.iso>.CONFIG` and is stored beside the ISO.
13. Confirm PS2 Classics cannot use the plaintext local profile builder; only an exact matching encrypted ENC catalogue entry may be installed.
14. Confirm the UI states that generic CONFIG toggles do not unlock 60 FPS. Treat the 11 source-pinned FPS candidates, 12 widescreen profiles and 13 480p profiles as separate exact per-game options; verify a wrong serial or ELF hash never exposes Apply, and verify generated `0x0A` entries include the exact original-word guard. The Enhancer must display the detected serial/hash; the first CROSS arms a code patch, the second CROSS applies it, and CIRCLE or D-pad navigation cancels without changing CONFIG.
15. Reapply the exact same patch and verify the app reports ALREADY INSTALLED without changing CONFIG. Confirm a partial overlap or conflicting replacement word is rejected and leaves CONFIG unchanged.
15. On the physical PS3 Slim, test each FPS candidate separately and record actual frame rate, gameplay timing, audio, cutscenes, controller input, crashes and stability. A successful CONFIG write is not proof of 60 FPS.
16. For display scaling, test XMB `PS / PS2 Upscaler` Normal and Full Screen, then test `PS / PS2 Smoothing` On and Off. Record the TV-reported output mode and best combination per title; PS3 SCALE does not change these XMB settings or claim native HD rendering.
17. Do not mark any option as hardware-verified until the test is completed on the physical PS3 Slim.

## Other hardware targets after the Slim test

- Compatible CFW FAT
- Other CFW Slim revisions
- Compatible CFW Super Slim (where available; most Super Slim models do not support traditional CFW)

CI build and host tests are necessary but not sufficient. Actual RSX appearance, audio playback, webMAN behavior, Artemis USERLIST loading and game stability require physical-console validation.

## Region metadata and PS2 CONFIG match safety

1. Confirm BLUS / BCUS / NPUB / NPUD IDs display NORTH AMERICA.
2. Confirm BLES / BCES / NPEB / NPED IDs display EUROPE.
3. Confirm SLPM / SLPS / SCPS / NPJB / NPJD IDs display JAPAN.
4. Confirm SLKA / SCKA / BLKS / BCKS / NPKB IDs display KOREA.
5. Confirm BLAS / BCAS IDs display ASIA.
6. Confirm an unknown or malformed ID displays UNKNOWN and is never assigned a guessed region.
7. Confirm PS2 CONFIG lookup uses exact normalized Title ID or disc serial and never selects a different region's config.


## Region-aware FPS profile matching

1. Confirm a profile is available only for the exact scanned Title ID and APP_VER.
2. Confirm the profile's inferred region matches the scanned game's region.
3. Force a mismatched region in a host test while keeping Title ID and APP_VER unchanged; confirm the profile is rejected.
4. Confirm a different APP_VER is rejected even when Title ID and region match.
5. Confirm an unrecognized region is not guessed; exact Title ID and APP_VER are still required.


## Community NCL region safety

1. Confirm fuzzy title / same-region matches remain discovery suggestions only.
2. Confirm exact installation requires the same Title ID and APP_VER.
3. Provide an index record with a different regional Title ID and no occurrence of the game's exact Title ID in its path; confirm installation is rejected.
4. Provide a multi-region NCL path that explicitly lists the game's exact Title ID alongside another regional ID; confirm that exact ID may match.
4. Confirm US, EU, Japan, Asia and Korea region labels use the shared game-ID prefix map.


## Community NCL Git-blob integrity

1. Parse a valid 11-column record with a 40-character Git blob SHA-1.
2. Confirm legacy 8- and 10-column records remain readable but cannot authorize installation.
3. Verify the known Git blob vector for `hello\n` (`ce013625030ba8dba906f756967f9e9ca394464a`).
4. Change one byte while preserving file size; confirm hash verification fails.
5. Change the expected size; confirm verification fails before install.
6. Confirm the download worker deletes a hash-mismatched temporary NCL and does not create the Artemis destination file.
7. Confirm the index builder exports the SHA from each GitHub tree blob entry.


## Atomic persistence and download failure safety — Alpha 0.32

Host CI verifies:
1. Settings/favorites are replaced through a unique same-directory temporary file, fsync and atomic rename.
2. A destination symlink is replaced as a link; its target file remains unchanged.
3. Empty favorites data is saved as a valid zero-byte file.
4. Invalid destinations fail without leaving temporary files behind.
5. Profile script installation refuses an existing temporary symlink or regular file and preserves it byte-for-byte.
6. A foreign webMAN script reached through a symlink is not overwritten or deleted.
7. HTTPS download temporary files use unique PID/time/sequence names and O_EXCL.
8. Failed catalog, community NCL and PS2 CONFIG downloads do not unlink pre-existing destination files.
9. A downloaded file is removed only when the current worker created it and later validation fails.

Physical PS3 checks still required:
- Change music, volume, language, animation and enhancement-storage settings; exit and relaunch; verify every value persists.
- Add/remove favorites, exit and relaunch; verify the list is intact.
- Interrupt a network refresh, retry it, and confirm the previous valid cache remains usable.
- Repeat a PS2 CONFIG backup/restore and one verified NCL install on the target console.


## Support outbox and retry — Alpha 0.33

Host CI verifies:
- UUIDv4 request ID validation and stable JSON payload generation.
- JSON escaping for quotes, backslashes, tabs, newlines and UTF-8 Arabic text.
- Exact maximum title (100) and details (2,000) bounds plus over-limit rejection.
- Atomic JSON outbox writes with a unique filename per request.
- Pending queue discovery, request ID/file-name matching and bounded file reads.
- Help & Support exposes a Retry Pending action; successful server responses remove only the sent pending file.
- The public support endpoint validates input, enforces idempotency and uses atomic HMAC-based daily rate limiting.
- RLS and explicit grants prevent public/authenticated reads of support reports.
- Static Edge Function contract checks require a bounded 12 KiB streaming body reader, cancellation above the limit and rejection of invalid UTF-8.
- Locale tests cover Help & Support labels, the PSN privacy warning and delivery statuses in Arabic and Spanish.

Physical PS3 checks still required:
- Create a support report while offline, exit/relaunch, reconnect, and retry from Help & Support.
- Confirm a successful send removes the matching pending JSON file and does not resend it.
- Confirm a failed send leaves the pending JSON file intact.
- Confirm the endpoint returns a rate-limit message after the daily quota is reached.
- After deployment, confirm an oversized request returns HTTP 413 and malformed UTF-8 returns HTTP 400 before any report is stored.
- Submit the same request UUID twice and confirm the second response is marked duplicate without creating another report.
