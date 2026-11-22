# Phase 18 — Route the Clockwork vault blast to both local players

## Problem

The original `new_trench_setup` applied the vault blast damage, shellshock, prone stance and push vector only to `level.player`. It also played the artillery rumble only on Logan. A native secondary local player would therefore miss a shared mission event.

## Local Stage 5 source change

Added `clockwork_apply_vault_blast(player, blast_origin)` to the local Clockwork interior script.

- It validates that the supplied entity is a defined, living player and that the blast origin exists.
- It applies the original near-lethal blast effect to the supplied player, clamping the damage amount so it cannot become negative when health is five or less.
- It applies shellshock, prone stance and the push vector to that player.
- After 0.2 seconds, it clears the push vector only if that same player is still alive.
- `new_trench_setup` starts the helper for Logan and, when the native secondary alias is defined and alive, for `level.playengineeffects` as well.
- The artillery rumble is also sent to both valid local players.

This is a shared scripted blast, not an attempt to clone damage or invent a second player entity. If the engine has not created the native secondary, the secondary branch is skipped.

## Audit and tests

The read-only Clockwork source audit now checks that both local-player call sites exist and that the helper uses its explicit player argument instead of falling back to `level.player`. Negative tests remove the secondary call or inject a primary-only damage reference and expect the audit to block the source.

## Local build record

- GSC Tool 1.6.2.364
- IW6 / PS3
- `maps__clockwork_interior.gscbin`: 23,168 bytes
- SHA-256: `03a99584a04a1c8018f7904c7b0b674d838d1bf46e9f24ef457c93eb01b706d7`
- Decompile and recompile after decompile: passed
- Original and round-trip GSCBIN: byte-identical

## Runtime boundary

The static patch does not prove that the blast timing, damage, shellshock, prone transition, push vector or rumble are correct in a live two-player Campaign session. Those effects must be checked on PS3/RPCS3 after native Player 2 creation is proven. No FastFile rebuild, signing or PKG is claimed.
