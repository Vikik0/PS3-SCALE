# Phase 33 — Player-state ownership audit

## Purpose

The Phase 32 call graph shows which selected GSC functions are connected to script entry candidates. Phase 33 classifies their visible player API calls, player-list references, secondary-client references, level-scope fields, mission flags and dvar calls.

## Tool

tools/gsc_player_state_ownership_audit.py imports the shared GSC parser from gsc_callgraph_audit.py and scans selected high-priority functions in Black Ice, Carrier and Cornered.

It reports:

- player method calls grouped into input, weapons, movement/control, camera, HUD, health/lifecycle and feedback/audio;
- direct player-field references;
- player-list references (level.playerpiggyback, level.players);
- secondary-client references (level.playengineeffects) separately from arrays;
- level-scope field references, flag API calls and dvar API calls;
- a conservative ownership-pattern label and explicit warning.

A level.* field is not automatically classified as globally shared at runtime. It is reported as a level-scope field for manual ownership review.

## Windows usage

Run from experiments/ghosts-campaign-coop after extracting the Phase 31 analysis bundle:

~~~powershell
python tools/gsc_player_state_ownership_audit.py black_ice="D:\Ghosts\Phase31\missions\black_ice\decompiled_gsc" carrier="D:\Ghosts\Phase31\missions\carrier\decompiled_gsc" cornered="D:\Ghosts\Phase31\missions\cornered\decompiled_gsc" --output-json "D:\Ghosts\analysis\phase33-player-state-ownership.json"
~~~

## Important semantic distinction

The previous PS3 common-loader audit recorded that level.playerpiggyback is populated from existing player entities, level.player points to the first, and level.playengineeffects stores the second entity reference when one already exists. That loader enumerates entities; it does not create the second local client or bind Controller 2. The ownership audit therefore treats playengineeffects as a secondary-client reference, not a player array.

## Validation boundary

The tool is read-only. Five synthetic tests cover method classification, player-array detection, secondary-client distinction, comment/string filtering and missing targets. No mission GSC is rewritten and no second Campaign local client is claimed.
