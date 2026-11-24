# Device test matrix — Alpha 0.31

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
11. Launch the same PS2 game with and without the CONFIG and record actual behavior; do not infer compatibility from a successful file install alone.

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
3. Provide an index record with a different regional Title ID while its path contains the game's Title ID; confirm installation is rejected.
4. Confirm US, EU, Japan, Asia and Korea region labels use the shared game-ID prefix map.
