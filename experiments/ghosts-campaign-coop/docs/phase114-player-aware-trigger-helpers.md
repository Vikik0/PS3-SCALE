# Phase 114 — player-aware mission trigger helper prototypes

## Scope

This phase creates isolated helper variants from the exact original decompiled IW6/PS3 mission utility sources. The existing function signatures remain as compatibility wrappers; the new variants accept an explicit player entity. No mission call site is migrated in this phase.

## Pinned source inputs

| Mission source | SHA-256 | Bytes |
|---|---|---:|
| Black Ice `maps/black_ice_util.gsc` | `3de920d152d16b5c70673d322ce2f9d65e22fab5a88ba7a4c7dad3291828c355` | 51,474 |
| Carrier `maps/player_scripted_anim_util.gsc` | `40886bdad63803469e7e54bfd565ebe8fea61aa8643d2a4ec9d4be385bb76b0e` | 2,303 |
| Cornered `maps/player_scripted_anim_util.gsc` | `40886bdad63803469e7e54bfd565ebe8fea61aa8643d2a4ec9d4be385bb76b0e` | 2,303 |

Carrier and Cornered use byte-identical copies of the same shared helper source.

## Changes

- Black Ice: added `waittill_trigger_for_player(player, ...)`; all player-specific aim, stance, movement and use-button checks inside the new helper use the supplied entity. The old `waittill_trigger(...)` signature remains and delegates to `level.player`.
- Carrier and Cornered: added `waittill_trigger_for_player(player, ...)` and passed that same player into the trigger callback.
- The shared trigger callback now waits for `"trigger"` with its activator argument. It ignores undefined activators and events whose activator is not the explicit player being watched. This prevents one player's trigger event from being accepted by another player's watcher.
- The old generic helper remains available for original single-player call sites.

## Local compiler validation against the supplied original source files

GSC Tool **1.6.2.364** compiled the modified source variants, decompiled each compiled GSCBIN, and recompiled the decompiled source byte-for-byte.

| Output | Source SHA-256 | Compiled GSCBIN SHA-256 | Bytes |
|---|---|---|---:|
| Black Ice helper | `35e4066c37d693d4041ebd76568ab7a89a193488ccf8b1f29ffdce27adc8ab0c` | `e24db1a926f8e630a62785cf8ae4816ead5028e04c5d96e62e5c096ebb886255` | 12,043 |
| Carrier / Cornered shared helper | `e28b43675f8879f0db1e4883c4c0425bc78038e3b41299acaa22c4522d533699` | `f82011abe291d8ec2b66b5c7069ec70e68693243d953c72be041ebe9ab5f45bb` | 646 |

These are local compile/decompile/recompile results from the supplied mission archive. The GitHub CI also compiles and round-trips the isolated helper fixtures; it does not fetch the user's private original FastFiles.

## Boundary

The helper variants are experimental source/build outputs only. The 15 original generic `waittill_trigger` call sites across Black Ice, Carrier and Cornered have not been migrated. No original FastFile, EBOOT, SELF or retail mission asset was modified or rebuilt. No physical PS3 runtime test is claimed. Player 2 remains **NOT_PROVEN** and the four original shared mission interactions remain **0/4 ready**.
