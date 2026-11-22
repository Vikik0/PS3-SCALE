# Phase 16 — Clockwork drill owner handoff and sequential stage correction

## Source-level discovery

The original Clockwork mission calls `handle_drill_spot` from `new_trench_setup` twice, in this order:

1. `drill_spot1` with `drill1_start` / `drill1_complete`;
2. only after its post-drill animation and the `drill_spot2_ready` flag, `drill_spot2` with `drill2_start` / `drill2_complete`.

Both stages use the same vault-door rig, mission-wide drill flags, drill-progress state and PIP object. The retail script does not start two independent drill sessions concurrently. The earlier portable model that allowed P1 and P2 to own different spots at the same time was therefore too permissive and has been corrected.

## Shared ownership model correction

Updated `client/ps3/campaign_interaction.h`, `campaign_interaction.c`, and the host tests:

- exactly two Clockwork drill stages are modelled;
- only one active drill owner is allowed globally;
- Stage 2 is rejected until Stage 1 has been completed;
- the player who owns Stage 2 may differ from the player who completed Stage 1;
- death, disconnect or cancel releases the active stage without setting its completion bit;
- either validated local player may retry the incomplete stage.

Updated `ghc_campaign_local_session.gsc` to apply the same sequential guard in `ghc_campaign_drill_begin`. The helper now rejects a second active stage even when it belongs to the other local player, and refuses Stage 2 until Stage 1 is complete.

## Local retail-source patch: Stage 3

The local Stage 3 patch extends the Stage 2 Clockwork source:

- `handle_drill_spot` returns the actual player who completed the current stage on both its normal and too-far exits.
- `new_trench_setup` stores that return value separately for Stage 1 and Stage 2.
- The `back1` and `back2` post-drill animation links, ADS lock and unlink now target the player who actually operated that stage, rather than unconditionally targeting `level.player`.
- The drill-plant sound helper receives the selected player and plays the sound on that player after the original 0.25-second delay.
- The drill-pullout sound is played directly on the `player` argument in `handle_misc_drill_details`, instead of using the stock audio helper that hard-codes `level.player`.
- The existing Logan weapon restoration and final mission cleanup remain on the primary player; the secondary player's original weapon is restored by the player-scoped drill cleanup.

## Local GSC build record

- GSC Tool: 1.6.2.364
- Target: IW6 / PS3
- Source file: `maps__clockwork_interior.gsc`
- Compiled GSCBIN: 22,954 bytes
- Compiled GSCBIN SHA-256: `544eb1de19ca10f46921985c2009b72be38fa6332f7f3c5b08fb626621b9d08f`
- Decompile: passed
- Recompile after decompile: passed
- Round-trip GSCBIN SHA-256: `544eb1de19ca10f46921985c2009b72be38fa6332f7f3c5b08fb626621b9d08f`
- Result: byte-identical round trip.

## Remaining player-specific work

At the end of Stage 3, the drill PIP still used the stock shared `level.pintoscreenedge` path. Stage 4 subsequently added a player argument to the local PIP helper and routes `drill_player` into it; see Phase 17. Independent per-viewport HUD rendering remains unproven until runtime testing.

The portable C model and GSC bridge remain separate from the local retail-source patch. The bridge is not injected or automatically loaded by the retail Campaign VM, and the local `handle_drill_spot` patch does not yet call the bridge ownership API.


## Latest CI validation

GitHub Actions run `45070589371` passed all five jobs. The Python suite passed 197 tests, including the sequential Campaign interaction tests, the Clockwork owner-handoff source audit, and the PS3 MapEnts synthetic-fixture tests. The pinned GSC Tool compile/round-trip, PS3 PRX bootstrap, protocol header and worker checks also passed. This remains source/build validation, not a PS3 runtime test.

## Validation boundaries

- Host tests validate the sequential ownership contract and source-audit checks.
- GSC compilation and byte-identical round trip validate script syntax/bytecode integrity only.
- The exact MapEnts trigger class is still unconfirmed because the supplied files do not include `maps/clockwork_interior.d3dbsp` or `.ents`.
- Native Player 2 creation in Campaign, controller routing, independent camera/HUD, death/respawn, checkpoint and mission-transition behavior remain unverified on PS3/RPCS3.
- No FastFile rebuild, signing or installable PKG is claimed.

Do not treat Stage 3 as a playable co-op release until the map trigger data and native runtime gates have been tested.
