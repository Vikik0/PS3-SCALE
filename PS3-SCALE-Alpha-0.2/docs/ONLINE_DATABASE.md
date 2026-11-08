# Online enhancement catalogue — Alpha 0.2

## Current implementation

PS3 SCALE bundles a starter catalogue at **data/online_profiles.tsv** and can refresh it from the project's HTTPS raw-file endpoint. The current starter catalogue contains 40 community-reported PS3 records.

The TSV fields are:

1. **TITLE_ID** — exact nine-character PS3 product code.
2. **APP_VER** — exact game update version in NN.NN format.
3. **TITLE** — display name only; never used as the identity key.
4. **EFFECT** — reported effect, such as an FPS cap change.
5. **TRUST** — B_COMMUNITY for community-reported profiles; B_COMMUNITY_MANUAL for source entries that require in-game timing/reapplication and are excluded from one-launch automation.
6. **PATCHES** — semicolon-separated address,expected_hex,replacement_hex records.
7. **NOTES** — source confidence and limitations.
8. **SOURCE_URL** — HTTPS source reference.

This is a data catalogue, not a collection of downloaded executable files or EBOOT binaries. The app downloads only the small TSV catalogue; it does not fetch arbitrary patch binaries or run downloaded executables.

## Synchronization and cache

- HTTPS peer and host verification are enabled using the bundled CA certificate file.
- The response is capped at 64 KiB and written to a temporary download file.
- The complete catalogue must pass strict parsing, exact identity validation, patch syntax validation and duplicate detection before replacing the current cache.
- The previous valid catalogue remains available if the network request or validation fails.
- When the user opens Online Graphics Database, the app automatically refreshes if the local cache is older than 24 hours. START on that screen requests a manual refresh. Normal app startup and game launching remain offline.
- A normal HTTPS catalogue does not require a PSN account.

## Matching and application limits

- A profile matches only when both Title ID and APP_VER are exact. Title names alone never select a patch.
- The starter catalogue contains 33 one-launch-eligible B_COMMUNITY records, 6 multi-write B_COMMUNITY records that remain disabled, and 1 B_COMMUNITY_MANUAL record (Brütal Legend) that requires in-game reapplication and is excluded from automation. All are community reports, not profiles independently hardware-verified by PS3 SCALE.
- An eligible single-write B_COMMUNITY profile can be armed only after the exact Title ID + APP_VER is mounted at /dev_bdvd and the user explicitly confirms. Manual-only and multi-write profiles cannot be armed by this build. webMAN MOD and its PS3MAPI endpoint must respond.
- Multi-write profiles remain disabled for automatic application in this build.
- The generated webMAN script is one-launch only. It checks the Title ID, in-game state, exact target address and expected original bytes, then deletes itself after the launch attempt. Before allowing Start Game, PS3 SCALE compares the complete script body against the deterministic generated script; a matching header alone is not trusted, and any manual body edit is treated as stale.
- The script does not show a success popup. webMAN's batch command does not provide PS3 SCALE with a verified post-write memory value, so the app must not claim that the patch succeeded.
- The patch changes the running game's memory only. It does not modify the original game folder, EBOOT, ISO or update files. Closing the game ends that process and clears its in-memory patch.

## Known limitations

- The webMAN batch language does not expose a direct APP_VER condition. PS3 SCALE checks APP_VER before arming and again against the mounted game when Start Game is used; the script additionally checks the exact original bytes at one address. Do not arm a profile and then update the game before launching it.
- Community-reported values can still be wrong for a particular console or game build. Start with one profile at a time and test stability.
- No universal upscaler, frame generation, or guaranteed 60 FPS is provided.
- A future binary/file-based patch catalogue would require per-file SHA-256, size, author/license, backup and rollback metadata before those patch types are enabled.
