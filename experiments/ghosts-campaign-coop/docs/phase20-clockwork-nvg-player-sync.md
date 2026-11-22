# Phase 20 — Clockwork NVG lever and player-owned light FX

## Goal

Stage 6 routed the interior loadout, vault inventory and CQB controls to the validated secondary alias, but the NVG light/lever path still used a single shared \`level.nvg_light_on\` flag and hard-coded \`level.player\` pose. This stage makes the light FX state belong to the player who owns it, while preserving the existing no-argument \`player_lever_input()\` entry point used by the retail mission scripts.

## Stage 7 source changes

Updated the user's pinned \`maps__clockwork_interior_nvg.gsc\` source:

- \`player_lever_input()\` still accepts no arguments, so existing calls from the main Clockwork script and \`handle_blackout()\` remain unchanged.
- The entry point starts the light-FX worker for Logan and for \`level.playengineeffects\` only when that alias is defined, is a player, is alive and is distinct from Logan.
- A guarded watcher keeps checking for a late native secondary-player binding until the shared \`lights_on\` mission flag is set. It revalidates the current alias each pass and does not create a duplicate worker for a player that already owns the effect.
- New \`player_nvg_light_fx(player)\` creates the effect for the explicit owner and follows that player's own angles and origin. It no longer reads Logan's pose for the secondary light.
- The duplicate-worker guard and NVG on/off state are stored on the owning player entity (\`ghc_nvg_light_active\`, \`ghc_nvg_light_on\`). The old shared \`nvg_light_on\` flag is no longer used by these workers.
- \`keep_player_below_surface(player)\` polls that player's \`nightvision_effectsoff\` state. The worker and its FX exit when the shared lights-on event occurs or the owner is no longer alive/defined; the FX entity and per-player state are cleaned up.

The main mission call sites are intentionally left unchanged. The wrapper dispatches to both players, which avoids editing another retail script merely to add an argument.

## Source identity and local build

- Input source SHA-256: \`6d4617f65e5fd0ba91f85ebf6e2b35a57901cc1ebdce85c8899c8781815cf096\`
- Patched source: 47,250 bytes; SHA-256 \`d7e07d1c7886ecd156a83c2d949525b9d2a348f702d53d09aad1d0d0f37ed16e\`
- GSC Tool: \`1.6.2.364\`
- Target: IW6 / PS3
- Compiled \`maps__clockwork_interior_nvg.gscbin\`: 12,392 bytes; SHA-256 \`575a23c17b4963aaf10e31797e6414f16986dc41094cc510915dfe3cf6d5b1f6\`
- Decompile/recompile round trip: passed; original and round-trip GSCBIN are byte-identical.
- Read-only source audit: 20/20 checks passed.
- New host tests: 18/18 passed.
- A synthetic GSC fixture containing the new helper functions also compiles and round-trips byte-identically in GSC Tool 1.6.2.364.

## Safety and compatibility behavior

- If no native secondary player exists, only Logan's original light path runs.
- If Player 2 is bound later, the watcher can attach the light worker without restarting the mission.
- If the secondary alias changes after a death/rebind, the watcher uses the current entity and the old worker exits when its owner is no longer alive.
- The patch does not spawn a second player and does not change the mission's shared \`lights_on\` progression flag.
- The legacy \`nvg_light_on\` flag may still be set by the main Clockwork script; it is now unused by the Stage 7 per-player light workers and is retained for compatibility with unmodified assets.

## Remaining validation gates

This is source/build validation only. It does not establish that the retail engine updates \`nightvision_effectsoff\` independently for both local players, that the flashlight FX renders in both split-screen viewports, or that a native Campaign Player 2 is actually created. Those require a live Campaign session on PS3 or RPCS3 with two controllers.

No FastFile was rebuilt or signed, and no installable PKG was created. Do not replace the retail FastFile with this GSCBIN directly.

## Next engineering pass

The next static adaptation target is the Clockwork interior NVG/AI flow: verify which enemy-acquisition and threat interactions should be shared versus player-scoped, then cover the player-facing lever input and light response with runtime captures. Avoid blindly duplicating \`getenemyinfo(level.player)\` or scripted AI target assignments; those can change mission AI behavior if duplicated.
