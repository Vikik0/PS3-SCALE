# Game enhancement lifecycle — Alpha 0.23

## User-visible sequence

1. The user installs or copies their own game using multiMAN or webMAN.
2. The user opens PS3 SCALE and scans the PS3 library.
3. PS3 SCALE reads the game's exact Title ID and APP_VER from its metadata when available.
4. Online DB shows the last valid local catalogue. On its first open in a session, it starts a real HTTPS refresh automatically. Triangle forces another refresh; Select opens search; Square changes the result filter.
5. A memory profile is displayed only for an exact Title ID + APP_VER match. Its effect, community trust level and source are shown.
6. Before arming an eligible single-write B_COMMUNITY profile, PS3 SCALE revalidates the exact installed HDD title's Title ID + APP_VER from PARAM.SFO, or verifies the exact mounted /dev_bdvd Title ID + APP_VER for disc/folder titles. The user then confirms with SQUARE followed by TRIANGLE. Multi-write profiles and manual-only profiles are not automated. No patch is silently armed.
7. PS3 SCALE checks webMAN MOD and PS3MAPI on a worker thread before writing the per-title memory script, keeping the controller UI responsive during network delays.
8. Start Game runs in a worker thread. Installed PS3 HDD/NPDRM titles launch through webMAN's `/play.ps3?<TITLE_ID>` route. PS3 folder and ISO entries launch through `/play.ps3<URL-encoded-game-path>`; an already-mounted /dev_bdvd title uses `/play.ps3`.
9. If PS3MAPI is unavailable when an armed memory profile is about to launch, PS3 SCALE removes its own one-launch script and still starts the game unmodified. If webMAN's local HTTP endpoint itself is unavailable, the app reports the network/HTTP failure and stays responsive.
10. Separately, Cross in Online DB installs an eligible community NCL file only for a scanned HDD game with an exact Title ID + APP_VER match, a `GRAPHICS_FPS` category, and an installed Artemis PS3. The file is placed in the detected Artemis USERLIST directory; it is not executed by PS3 SCALE.
11. Temporary NCL files are the default and are removed the next time PS3 SCALE starts. Permanent storage is available in Settings. The user opens Artemis and selects the installed NCL code before launching the game.
12. Game Details loads a before/after screenshot pair only when both PNG files match the game's Title ID and APP_VER. If no pair exists, the app shows a missing-preview state rather than a concept illustration.

## Current architecture

- **PS3 SCALE manager:** library scanning, exact identity matching, HTTPS catalogue sync, cache, profile review, confirmation, NCL download and Start Game.
- **webMAN MOD runtime:** executes a per-title one-launch memory batch script after the game starts.
- **Artemis runtime:** loads user-selected NCL files from USERLIST. PS3 SCALE only transfers an eligible text file; Artemis applies the selected codes.
- **Preview loader:** reads two local PNG captures per Title ID + APP_VER. The repository does not currently bundle screenshot pairs for every game.
- **PS2 path:** PS2 CONFIG preview remains separate; no PS3 memory profile or NCL graphics patch is applied to PS2 titles.

## Safety and failure handling

- Keep original game files read-only. This implementation does not patch game files.
- Never match by title name alone.
- A memory profile or installable NCL entry must match the exact Title ID and APP_VER.
- The webMAN script searches exactly one possible start address and checks the expected original bytes before writing.
- Multi-write profiles are disabled for automatic application.
- The script removes itself after one launch attempt to reduce stale reuse.
- If a PS3 SCALE-owned script is stale, its patch fingerprint differs from the current catalogue, or its body has been manually edited, Start Game is blocked until the script is removed or regenerated. The full script body is checked, not only its header.
- If a user-owned webMAN script exists, PS3 SCALE does not overwrite or delete it.
- If no exact memory profile exists, or PS3MAPI is unavailable, the game starts unmodified when webMAN can be reached.
- The memory patch is process-only; exiting the game clears it. Remove a pending script with SQUARE before launch.
- The webMAN batch language has no direct APP_VER condition. PS3 SCALE checks APP_VER before arming and checks the mounted game's APP_VER when Start Game is used. Do not update the game between arming and launching.
- NCL files are community-supplied. The installer accepts only small text `.ncl` files tagged `GRAPHICS_FPS`, does not overwrite existing files, and does not auto-apply them. Review the source before using Artemis.
- Temporary NCL files are cleaned on the next PS3 SCALE startup; permanent files remain until manually deleted.
- No success notification, universal AI upscaler or frame-generation claim is made.

## Current status

The Online DB refresh/search flow, Artemis NCL installer, temporary/permanent storage setting, local preview-pair loader and webMAN launch fallback are implemented on the PR #16 development branch. CI must pass after the latest commit, followed by a physical-console smoke test. Community reports and screenshots are not independently verified by PS3 SCALE.
