# Phase 64 — Trace exact player-container functions through the GSC call graph

## Purpose

Phase 60 inventories 13 functions containing Campaign player-container markers across Carrier and Cornered. Phase 32 already builds a file-aware call graph, but its default target selection is name-based. Phase 64 connects the Phase 60 manifest to the call graph using exact relative file path plus function name.

## Changes

- Add --target-manifest to gsc_callgraph_audit.py.
- Load the Phase 60 JSON manifest and validate every relative POSIX path and function identifier.
- Select a target only when both file path and function name match exactly.
- Report missing or duplicate-in-file targets explicitly; never fall back to another same-name function.
- For each exact target, report incoming direct/thread/namespaced call sites, callback registrations, direct reachability and callback-inclusive reachability from script-entry candidates.

## Windows usage

~~~powershell
python tools/gsc_callgraph_audit.py black_ice="D:\Ghosts\Phase31\missions\black_ice\decompiled_gsc" carrier="D:\Ghosts\Phase31\missions\carrier\decompiled_gsc" cornered="D:\Ghosts\Phase31\missions\cornered\decompiled_gsc" --target-manifest "reports\PHASE60_PLAYER_CONTAINER_PATHS_SUMMARY.json" --output-json "D:\Ghosts\analysis\phase64-player-container-callgraph.json"
~~~

## Validation boundary

Synthetic fixtures test duplicate names in separate files, missing exact targets, manifest path traversal rejection and exact target loading. This is static source reachability only. A callback reference does not prove runtime invocation; no native Campaign second player is claimed.
