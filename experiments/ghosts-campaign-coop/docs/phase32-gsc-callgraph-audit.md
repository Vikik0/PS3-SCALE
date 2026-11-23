# Phase 32 — GSC call-site and callback reachability audit

## Purpose

Phase 31 counted primary-player references and identified some player-array-aware code. Phase 32 adds a static call graph to determine whether those functions are referenced from extracted script entry candidates and whether a path depends on callback registration.

## Tool

tools/gsc_callgraph_audit.py scans decompiled .gsc source files and reports:

- function definitions and source file/line ranges;
- direct calls, thread calls, namespaced calls, and ::function callback references;
- exact namespace-path matches, same-file matches, unique-name fallbacks, and ambiguous references;
- reachability from magicreloadwhenreachenemy script-entry candidates, both with and without callback edges;
- call-site evidence for selected Player 2 / primary-player dependency functions.

The analyzer does not guess when a symbol is ambiguous. Namespaced calls resolved by unique-name fallback are marked as lower-confidence.

## Windows usage

Run from experiments/ghosts-campaign-coop after extracting the Phase 31 analysis bundle:

~~~powershell
python tools/gsc_callgraph_audit.py black_ice="D:\Ghosts\Phase31\missions\black_ice\decompiled_gsc" carrier="D:\Ghosts\Phase31\missions\carrier\decompiled_gsc" cornered="D:\Ghosts\Phase31\missions\cornered\decompiled_gsc" --output-json "D:\Ghosts\analysis\phase32-callgraph.json"
~~~

## Reachability semantics

- Direct reachability follows direct, thread, and namespaced calls.
- Callback-inclusive reachability also follows ::function references used for callback registration.
- A callback edge proves that the source registers/references a function pointer. It does not prove the engine invoked that callback during a particular run.
- magicreloadwhenreachenemy functions are treated as script-entry candidates because the extracted GSC set uses them as asset entry routines. Their actual runtime invocation remains a game-engine behavior, not something static source analysis can prove.

## Phase 32 evidence

The compact source-derived findings are in reports/PHASE32_CALLGRAPH_SUMMARY.json; the Arabic analysis is in reports/PHASE32_REPORT_AR.md.

## CI result

GitHub Actions run 45250534837 passed all five jobs: 290 Python tests, GSC script compile, PS3 PRX bootstrap, worker check and protocol header validation.

## Validation boundary

The tool is read-only and does not change GSC or FastFile inputs. Tests use synthetic GSC fixtures. The report is static call-graph evidence only; it does not establish a second native Campaign local client, controller routing, independent camera, or HUD.
