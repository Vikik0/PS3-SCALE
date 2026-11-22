# Phase 21 — Clockwork enemy target attribution

## Finding

The uploaded 'clockwork(1).ff' was re-extracted and its maps/clockwork_interior ScriptFile was decompiled with GSC Tool 1.6.2.364. The result is byte-identical to the previously audited 91,543-byte source (SHA-256 7fc0f02476d5c9ebbe7c28e68abe762a26a976a28587228c5027d1a5c543e1f1).

The Clockwork mission sample contains one direct enemy-to-player target assignment in discovery_guys_wakeup().

Other nearby code must not be misclassified:

- maps__clockwork_code.gsc::attack_targets() assigns the ally AI group to enemy AI targets. This is shared mission combat orchestration and must remain a single operation.
- maps__clockwork_interior_nvg.gsc::begin_interior() selects the closest axis AI to Logan to queue a scripted dialogue line. It is not an enemy target assignment.
- maps__clockwork_code.gsc::listen_player_press_x() compares Logan's position with a snowmobile entity to choose an audio crash mix. It is an audio cue, not AI aggro.

## Stage 8 patch design

The function-only patch changes only the target call inside discovery_guys_wakeup() and inserts clockwork_pick_enemy_target(ai) immediately before it.

For each discovery AI, the helper:

1. Rejects an undefined/dead AI entity.
2. Uses Logan if he is a valid live player.
3. Considers level.playengineeffects only while issplitscreen() is true.
4. Requires the secondary alias to be a live player, distinct from Logan, recognized by issplitscreenplayer(), and not recognized as the primary split-screen player.
5. Chooses the closer valid player using the AI and player origins.
6. Returns undefined if neither valid player exists, so the caller skips getenemyinfo() rather than passing an invalid entity.

This is a one-time target choice when the discovery AI wakes up. It is not a per-frame retargeting loop. It does not duplicate attack_targets, change story dialogue/audio ownership, or create Player 2.

## Stage 5/6 source lineage

Stage 5 and Stage 6 are actual Clockwork GSC adaptation stages, documented in Phase 18 and Phase 19. Their full merged source file and exact patch diff were not retained in the available working inputs.

The original Stage 8 proof was therefore applied to the Stage 2 source only; it was not a combined Stage 5/6 build. Phase 23 now adds a fail-closed reconstruction of the documented Stage 5/6 behaviors on top of the pinned Stage 2 source, then applies Stage 8 to that reconstructed Stage 6 source.

This reconstruction is behavior-based, not a recovered historical file. Its compiled sizes differ from the older Phase 18/19 build notes, so it must not be described as byte-identical to those historical artifacts.

## Validation

- Uploaded retail source decompile: byte-identical to the previously audited source.
- Read-only AI targeting baseline audit: 5/5 checks passed.
- Stage 8 host tests: 11/11 passed.
- Updated synthetic helper fixture: compile, decompile and byte-identical recompile passed.
- Reconstructed Stage 5, Stage 6 and combined Stage 8 sources: GSC Tool compile/decompile/recompile passed; each GSCBIN was byte-identical after round-trip.
- Stage 5/6 source audit: 13/13 checks passed.
- Stage 5/6 reconstruction tests: 7/7 passed.

## Runtime gates

A PS3/RPCS3 Campaign test is still required to establish that getenemyinfo() produces the expected aggro behavior for both native local players, that the engine accepts the secondary entity as a valid enemy target, and that the discovery encounter remains stable after player death/rebind. The nearest-player choice occurs once at wake-up; dynamic retargeting after players move is not implemented.

No retail FastFile was rebuilt or signed, and no PKG was created.
