# Phase 65 — One-command, source-pinned player-container analysis

## Purpose

Phase 64 connects exact Phase 60 file/function targets to the GSC call graph. Phase 65 removes the manual handoff: one runner executes the ownership audit, derives exact targets from its full player_container_functions result, and runs the call graph against the same mission source roots.

## Changes

- Add tools/run_player_container_analysis.py.
- Hash every .gsc source file and build a deterministic per-mission tree SHA-256 before analysis.
- Run the ownership audit and derive exact relative-file/function targets directly from its findings.
- Run the file-aware call graph against those exact targets.
- Re-hash all source files after both audits and reject the result if any file was added, removed or changed.
- Write the combined JSON report atomically; refuse to overwrite an existing report unless --overwrite is explicit.
- Reject symbolic-link GSC inputs so the source manifest cannot silently escape the selected mission directory.

## Windows usage

~~~powershell
python tools/run_player_container_analysis.py black_ice="D:\Ghosts\Phase31\missions\black_ice\decompiled_gsc" carrier="D:\Ghosts\Phase31\missions\carrier\decompiled_gsc" cornered="D:\Ghosts\Phase31\missions\cornered\decompiled_gsc" --output-json "D:\Ghosts\analysis\phase65-player-container-analysis.json"
~~~

## Output

The report contains the exact source-file hashes, ownership findings, exact target call sites, callback references, direct reachability and callback-inclusive reachability. It records missing/ambiguous targets rather than silently substituting another function.

## Validation boundary

Synthetic tests exercise the complete orchestration, duplicate-name handling, source mutation detection, symbolic-link rejection, deterministic source manifests and output safety. This remains static GSC evidence; it does not prove that a callback executes or that Campaign can initialize a second native local player.
