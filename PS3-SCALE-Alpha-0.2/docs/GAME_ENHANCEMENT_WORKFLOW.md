# Game enhancement lifecycle — Alpha 0.2

## User-visible sequence

1. The user installs or copies their own game using multiMAN or webMAN.
2. The user opens PS3 SCALE and scans the PS3 library.
3. PS3 SCALE reads the game's exact Title ID and APP_VER from its metadata when available.
4. It loads the bundled/cached catalogue and optionally synchronizes the HTTPS catalogue.
5. A profile is displayed only for an exact Title ID + APP_VER match. Its effect, community trust level and source are shown.
6. Before arming an eligible single-write B_COMMUNITY profile, PS3 SCALE revalidates the exact installed HDD title's Title ID + APP_VER from PARAM.SFO, or verifies the exact mounted /dev_bdvd Title ID + APP_VER for disc/folder titles. The user then confirms with SQUARE followed by TRIANGLE. Multi-write profiles and manual-only profiles such as Brütal Legend are not automated. No patch is silently armed.
7. PS3 SCALE checks webMAN MOD and PS3MAPI on a worker thread before writing the per-title script, keeping the controller UI responsive during network delays.
8. Start Game runs in a worker thread so a slow or unavailable webMAN service cannot freeze controller input. Installed PS3 HDD/NPDRM titles launch through webMAN's /play.ps3?<TITLE_ID> route. PS3 folder and ISO entries launch through /play.ps3<URL-encoded-game-path>; an already-mounted /dev_bdvd title uses /play.ps3.
9. If a profile is armed, PS3 SCALE rechecks the installed HDD metadata or exact mounted disc version before launching. If no exact profile exists, or a profile has not been armed, the selected game still launches normally without a graphics patch.
10. webMAN runs /dev_hdd0/tmp/wm_ingame/<TITLE_ID>.bat for an armed title. The one-launch script checks the Title ID, in-game state, exact memory address and expected original bytes. It then deletes itself.
11. PS3 SCALE does not display a graphics-success popup. The current webMAN batch path cannot report a verified post-write value to the app. The user should test game stability and performance.

## Current architecture

- **PS3 SCALE manager:** library scanning, exact identity matching, HTTPS catalogue sync, cache, profile review, confirmation and Start Game.
- **webMAN MOD runtime:** executes a per-title one-launch batch script after the game starts.
- **PS2 path:** PS2 CONFIG preview remains separate; no PS3 memory profile is applied to PS2 titles.

## Safety and failure handling

- Keep original game files read-only. This implementation does not patch game files.
- Never match by title name alone.
- A profile is accepted only for an exact Title ID and APP_VER.
- The webMAN script searches exactly one possible start address and checks the expected original bytes before writing.
- Multi-write profiles are disabled for automatic application.
- The script removes itself after one launch attempt to reduce stale reuse.
- If a PS3 SCALE-owned script is stale, its patch fingerprint differs from the current catalogue, or its body has been manually edited, Start Game is blocked until the script is removed or regenerated. The full script body is checked, not only its header.
- If a user-owned webMAN script exists, PS3 SCALE does not overwrite or delete it.
- If no exact profile exists, the game starts unmodified.
- The patch is process-memory-only; exiting the game clears it. Remove a pending script with SQUARE before launch.
- The webMAN batch language has no direct APP_VER condition. PS3 SCALE checks APP_VER before arming and against the mounted game when Start Game is used. Do not update the game between arming and launching.
- No success notification, universal AI upscaler or frame-generation claim is made.

## Current status

The online catalogue and one-launch webMAN script workflow are implemented in the feature/online-graphics-database branch. The 40 starter entries are community-reported, not yet independently verified on the user's PS3. Host and PS3 package CI must pass, followed by a physical-console smoke test, before this branch is described as hardware-tested.
