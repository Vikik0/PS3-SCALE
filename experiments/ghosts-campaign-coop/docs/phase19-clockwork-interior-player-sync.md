# Phase 19 — Clockwork interior loadout, vault inventory and CQB player sync

## Goal

After the drill and vault blast work, the next source audit found several mission-progression actions still targeting only `level.player`. Stage 6 routes the key interior transitions to the validated secondary alias as well, while preserving primary-player behavior.

## Local Stage 6 source changes

Updated `maps__clockwork_interior.gsc`:

- `setup_interior_cqb` gives the secondary player the same CZ805 combat weapon, night-vision action slot, night-vision-off helper and forced-on goggles as Logan.
- `setup_interior` applies the post-vault interior combat weapon to the secondary player too.
- `vault_nag` shows the disable-NVG hint to both local players and waits until neither valid player is switching weapons.
- `animate_vault_door` sends the disable-NVG hint to the secondary player and clears that player's action slot along with Logan's.
- `vault_props` records the secondary player's frag/flash ammo, stores the exact secondary entity handle, and removes those grenades for the vault sequence.
- `animate_vault_door` restores the secondary player's saved grenade counts only if the current secondary alias still points to the exact entity that had its grenades removed. This avoids giving an old player's saved ammo to a replacement entity after a disconnect/rebind.
- `handle_cqb_allies` enables sprint and jump for the secondary player.
- `cqb_dog` sends the dog-attack override notification to the secondary player as well.

All secondary-player branches validate that `level.playengineeffects` is a defined, living player before using it. If the native secondary player does not exist, the stock Logan path continues without inventing a player.

## Static audit and tests

The read-only source audit now checks secondary loadout/NVG setup, vault hints and weapon-switch waiting, grenade save/remove/restore ownership, CQB movement permissions and dog-attack notification. Negative tests remove secondary-player behavior and require the audit to block the source.

## Local build record

- GSC Tool 1.6.2.364
- IW6 / PS3
- `maps__clockwork_interior.gscbin`: 23,919 bytes
- SHA-256: `4183af2a6fa7100d12cb48871172abdfd0136f6c84ffed0e4a63112d82cb4de4`
- Decompile/recompile round trip: pending final package verification

## Remaining interior-specific gaps

- `maps__clockwork_interior_nvg.gsc` still contains primary-only lever input and light-FX handling. It needs a separate player-aware adaptation so Player 2 can use the interior NVG lever and see the correct light effect.
- Enemy target acquisition and some scripted threat/interaction functions still use `getenemyinfo(level.player)` or hard-code `level.player`; these need a separate AI/interaction pass rather than blindly duplicating calls.
- The actual native Campaign Player 2, per-viewport HUD, death/respawn, checkpoint and transition behavior remain unverified on PS3/RPCS3.

No FastFile rebuild, signing or PKG is claimed by this phase.
