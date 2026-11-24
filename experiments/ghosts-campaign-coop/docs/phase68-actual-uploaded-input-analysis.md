# Phase 68 — Actual uploaded mission-input analysis

## Inputs verified

The three user-uploaded PS3 FastFiles match the pinned size, SHA-256, magic and TU version. Each Phase 31 extraction report also carries the same file hash and size. The extracted GSC tree contains 249 decompiled scripts and 4,304 parsed function definitions.

| Mission | GSC files | Function definitions | Player-container functions | playerpiggyback refs | playengineeffects refs |
|---|---:|---:|---:|---:|---:|
| Black Ice | 76 | 1,217 | 0 | 0 | 0 |
| Carrier | 118 | 1,537 | 4 | 9 | 4 |
| Cornered | 55 | 1,550 | 9 | 10 | 0 |

## Call-graph findings

- 13 exact file/function targets; no missing or duplicate target definitions.
- 14 incoming direct/thread/namespaced call sites and no direct function-pointer references to the target functions.
- 1 target is directly reachable from the current script roots; 8 are reachable when callback edges are included; 5 are not reachable from those static roots.
- The disable_stealth_system namespaced references use the analyzer's unique-symbol fallback because the namespace-to-file mapping is unresolved; treat those three sites as lower-confidence resolution.

## Strong split-screen clues

- Carrier monitorscavengerpickup checks whether level.playerpiggyback.size == 2 and assigns cg_playerFovScale0 / cg_playerFovScale1 to entries 0 and 1.
- Carrier get_different_favoriteenemy chooses a playerpiggyback entry based on comparison with level.playengineeffects.
- Carrier playerview_playmissanim uses level.playengineeffects and level.player as separate hideonclient selectors in the player-drone animation path.

These are meaningful per-client/split-screen clues, but they do not prove that playerpiggyback contains two native local players or that Campaign initializes Player 2.

## Extraction limitation

All three Phase 31 extraction reports say VALIDATED_PARTIAL. Black Ice and Carrier stop on invalid_or_non_deflate_trailing_data; Cornered stops on zero_terminator. The 249-script analysis therefore applies to the extracted GSC set and is not a claim that every byte/asset in each FastFile was fully extracted.

## Next step

Phase 69 will scan every available mission GSC file for assignments/initialization of level.playerpiggyback and level.playengineeffects, then separate script-side reads/writes from globals that appear to be populated outside the extracted mission scripts.
