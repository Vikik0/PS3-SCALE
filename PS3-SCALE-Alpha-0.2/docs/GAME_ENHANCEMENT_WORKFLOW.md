# Game enhancement lifecycle — requested workflow

## User-visible sequence

1. The user installs or copies their own game using multiMAN or webMAN.
2. The user opens PS3 SCALE.
3. The user selects Scan Games, chooses PS3 or PS2, searches if needed, and opens the game's details.
4. PS3 SCALE identifies the exact game (Title ID / PS2 serial), region and update version. It checks local profiles and the online catalog.
5. If a compatible enhancement exists, PS3 SCALE downloads the patch/config and validates its source, license, SHA-256, size, game identity and version.
6. Before any game-file change, PS3 SCALE creates and verifies a restorable backup. Prefer sidecar config or profile files that leave the original ISO/game data unchanged.
7. The app reports one of: Not supported, Downloaded, Prepared, Applied, or Failed. It must never call a prepared profile "Applied".
8. The user exits PS3 SCALE. A short PS3 confirmation beep may be played only after the preparation/apply step actually succeeds.
9. When the user launches the game, a separately loaded CFW plugin or title-specific runtime hook checks the running process and exact game identity. It applies only a matching, verified profile.
10. Only after the runtime hook verifies the expected patch bytes/state does it display the top-screen notification: "Graphics applied — Enjoy!"

## Required architecture

A normal GameOS manager process exits before the game begins rendering. It cannot itself remain alive to display a later in-game notification. The lifecycle therefore requires two cooperating components:

- **PS3 SCALE manager:** library scanning, exact title matching, catalog/download, backup, preparation, status and user interface.
- **Runtime component:** a CFW plugin/PRX or proven per-title hook that loads with the game, validates its identity/version, applies the selected memory patch and raises the notification.

For PS2 Classics/PS2 ISO, the compatible path is a per-title `ps2_netemu` CONFIG where the specific game and emulator support it. A sidecar CONFIG is preferred over modifying the ISO.

## Safety and failure handling

- Keep original game files read-only by default.
- Never patch based on title name alone.
- Verify Title ID/serial, region, update version, source, SHA-256 and expected original bytes.
- Backup before file modification and verify that the backup can be restored.
- Provide an explicit rollback operation.
- Do not display the success notification if the plugin is missing, the game identity differs, the patch does not match, or the post-apply check fails.
- If a title has no verified profile, tell the user that no compatible enhancement is available.
- No universal AI upscaling or frame generation is promised; only measured per-title improvements can be enabled.

## Current status

The current Alpha UI has library navigation, title search and a game-details page. It does not yet download/apply game patches, create game backups, install a runtime plugin, emit the requested exit beep, or display the in-game success notification. Those are later engineering milestones, not completed features.
