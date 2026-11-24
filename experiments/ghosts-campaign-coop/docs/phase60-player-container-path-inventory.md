# Phase 60 — Inventory every existing player-container path

## Scope

The 68-function hotspot audit is useful for high-density level.player code, but it does not cover every function that references the alternate player-container markers. Phase 60 scans every parsed function in all three mission GSC sets for:

- level.playerpiggyback
- level.players
- level.playengineeffects

Each hit is reported with its exact file, function, line range, direct level.player references and ownership classification.

## Results from the supplied extraction

- Black Ice: 0 functions with these markers.
- Carrier: 4 functions; 9 level.playerpiggyback references and 4 level.playengineeffects references.
- Cornered: 9 functions; 10 level.playerpiggyback references.
- Total: 13 functions, 19 playerpiggyback references, 4 secondary-client marker references, and no level.players references.

Important locations include Carrier 1031.gsc::monitorscavengerpickup, 987.gsc::getenemytarget, animscripts/dog/dog_combat.gsc::playerview_playmissanim and maps/ss_util.gsc::get_different_favoriteenemy; Cornered 987.gsc::getenemytarget, 1051.gsc::enable_stealth_system, 1051.gsc::disable_stealth_system, 1052.gsc::event_awareness_waitclear_ai_proc, 1054.gsc::system_message_loop, 1045.gsc::enemy_corpse_alert_level, 1050.gsc::enemy_threat_logic_dog_wait, and maps/cornered_interior.gsc::courtyard_rorke/bar_enemies.

## Changes

- The ownership audit now reports all player-container functions alongside the exact 68 hotspot targets.
- Summary counts reconcile per-marker references across each mission.
- A synthetic test verifies that player-container markers are discovered independently of ordinary level.player-only functions.

## Interpretation boundary

These are existing script-level data paths and useful places to study how the game handles player-like entities. They do not establish that playerpiggyback or playengineeffects is a second local controller, nor that Campaign can create a second native player.
