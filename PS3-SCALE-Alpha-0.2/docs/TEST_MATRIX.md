# Device test matrix — Alpha 0.28

Primary device (user-reported):
- Family: PS3 Slim
- System software: CFW 4.92
- Exact CECH retail model: not yet confirmed
- CEX / DEX: not yet verified on the console; the app queries this read-only when supported
- Cobra version and available HDD space: not yet confirmed

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
8. Confirm a stale or edited PS3 SCALE script is blocked, while a foreign webMAN script is never overwritten or deleted.
9. Confirm that closing the game clears the process-memory change.

## Other hardware targets after the Slim test

- Compatible CFW FAT
- Other CFW Slim revisions
- Compatible CFW Super Slim (where available; most Super Slim models do not support traditional CFW)

CI build and host tests are necessary but not sufficient. Actual RSX appearance, audio playback, webMAN behavior, Artemis USERLIST loading and game stability require physical-console validation.
