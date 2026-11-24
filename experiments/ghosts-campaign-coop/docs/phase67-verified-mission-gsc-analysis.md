# Phase 67 — Gate the mission source audit on exact FastFile identity

## Purpose

Phase 61–63 verify the three supplied mission FastFiles. Phase 65–66 run the ownership and call-graph analysis over extracted GSC sources. Phase 67 joins these gates so the GSC audit runs only when all three FastFiles match their pinned identities, and rechecks them after the analysis.

## Changes

- Add tools/run_verified_mission_gsc_analysis.py.
- Require all three roles: Black Ice, Carrier and Cornered FastFiles, plus their extracted GSC source directories.
- Verify the exact size, SHA-256, IWff0100 magic and big-endian version before the GSC analysis.
- Skip the ownership and call-graph stages if any FastFile does not match.
- Re-verify all three FastFiles after the GSC analysis and mark the report blocked if an input changed during the run.
- Prevent the output report from resolving to any FastFile or GSC source file.
- Write a single atomic JSON report containing FastFile identities and the source-pinned Phase 65 analysis.

## Windows usage

~~~powershell
python tools/run_verified_mission_gsc_analysis.py --black-ice-fastfile "D:\Ghosts\Phase31\black_ice.ff" --carrier-fastfile "D:\Ghosts\Phase31\carrier.ff" --cornered-fastfile "D:\Ghosts\Phase31\cornered.ff" --black-ice-gsc "D:\Ghosts\Phase31\missions\black_ice\decompiled_gsc" --carrier-gsc "D:\Ghosts\Phase31\missions\carrier\decompiled_gsc" --cornered-gsc "D:\Ghosts\Phase31\missions\cornered\decompiled_gsc" --output-json "D:\Ghosts\analysis\phase67-verified-mission-analysis.json"
~~~

## Validation boundary

Synthetic tests verify the identity gate, before/after recheck, mismatch blocking and output-path protection. The report is still static GSC evidence; it does not establish a live second Campaign player.
