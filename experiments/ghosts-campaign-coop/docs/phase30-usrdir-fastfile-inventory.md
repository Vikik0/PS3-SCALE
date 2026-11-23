# Phase 30 — full USRDIR FastFile and MapEnts inventory

## Why this phase exists

The earlier static pass concentrated on four supplied files: common.ff, clockwork.ff, patch_common.ff, and patch_common_mp.ff. The user's extracted PS3_GAME/USRDIR listing contains additional Campaign mission FastFiles such as black_ice.ff, carrier.ff, and cornered.ff. Those mission zones must be inventoried before concluding that Clockwork's missing MapEnts payload is unavailable in the dump.

## New offline scanner

Added tools/scan_usrdir_fastfiles.py. It recursively inventories .ff, .d3dbsp, .ents, .zone, .gsc, .csc, .gscbin, and .cscbin files under a supplied USRDIR folder.

For each .ff it:

- records the relative path, size and SHA-256;
- validates the known IW6 PS3 outer FastFile format and block stream;
- records incomplete/unsupported containers instead of silently skipping them;
- inspects the asset pool, including ScriptFile, RawFile and MapEnts record counts and pointer values;
- exports the names and hashes of structurally validated GSC candidates without modifying the input;
- searches the inflated zone for map path references such as maps/clockwork.d3dbsp;
- runs the current conservative script-string/MapEnts candidate scan and records errors separately from a confirmed zero-candidate result.

Standalone .d3dbsp and .ents files are scanned with the read-only MapEnts inspector. Files are never modified. The scanner does not claim that a MapEnts asset-pool record is a decoded entity string or a valid spawn list.

## Windows usage

Run from the repository's experiments/ghosts-campaign-coop directory:

~~~powershell
python tools/scan_usrdir_fastfiles.py "D:\Ghosts\BLUS31270\PS3_GAME\USRDIR" --output-json "D:\Ghosts\analysis\usrdir-fastfile-inventory.json"
~~~

The scan is recursive, so language subfolders are included. It has a default 2,000-file safety limit; raise --max-files only after reviewing the directory.

## Why it matters for Player 2

The previous Clockwork-only MapEnts scan found the maps/clockwork.d3dbsp path string and a MapEnts pool record, but no validated inline entity string. A full USRDIR inventory can identify whether the relevant map data is stored in a separate file, a different mission FastFile, or a container the current parser does not support. It also identifies additional Campaign mission script packages that need the same player-dependency audit.

## Validation boundary

CI uses synthetic FastFiles to test recursive discovery, profile parsing, map-path extraction, asset-pool reporting, and fail-soft handling of unsupported files. It does not claim that the user's complete USRDIR has been scanned in this environment. The tool is offline and does not require the PS3, PSN, or a live game session.

## CI result

GitHub Actions run 45200690532 passed all five jobs. The Python suite passed 280 tests, including four new recursive-USRDIR scanner tests. The PS3 PRX bootstrap build also passed. These are code/toolchain checks with synthetic FastFiles; they are not a scan of the user's local directory.
