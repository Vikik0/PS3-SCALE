# Phase 21 — Clockwork enemy target attribution

## Finding

The Clockwork mission sample contains one direct enemy-to-player target assignment:

- \`maps__clockwork_interior.gsc::discovery_guys_wakeup()\` calls \`getenemyinfo(level.player)\` once for each awakened discovery AI.

Other nearby code must not be misclassified:

- \`maps__clockwork_code.gsc::attack_targets()\` assigns the ally AI group to enemy AI targets (\`ally getenemyinfo(enemy)\`). This is shared mission combat orchestration and must remain a single operation.
- \`maps__clockwork_interior_nvg.gsc::begin_interior()\` selects the closest axis AI to Logan to queue a scripted dialogue line. It is not an enemy target assignment.
- \`maps__clockwork_code.gsc::listen_player_press_x()\` compares Logan's position with a snowmobile entity to choose an audio crash mix. It is an audio cue, not AI aggro.

## Stage 8 patch design

The new \`tools/clockwork_ai_target_patch.py\` changes only the \`discovery_guys_wakeup()\` target call and inserts \`clockwork_pick_enemy_target(ai)\` immediately before it.

For each discovery AI, the helper:

1. Rejects an undefined/dead AI entity.
2. Uses Logan if he is a valid live player.
3. Considers \`level.playengineeffects\` only if it is defined, is a player, alive and distinct from the selected primary.
4. Chooses the closer valid local player using the AI and player origins.
5. Returns that player; the caller invokes \`getenemyinfo()\` exactly once for the AI.
6. Returns undefined if neither native player is valid, in which case the caller skips the assignment instead of passing an invalid entity.

This is a one-time target choice when the discovery AI wakes up. It is deliberately not a per-frame retargeting loop. It does not duplicate \`attack_targets\`, change story dialogue/audio ownership, or create Player 2.

## Fail-closed patching

The patcher verifies the exact \`discovery_guys_wakeup()\` function-body SHA-256:

\`9003356c9a048664f9d09274d0146043df0fc011caae2a83555b4335ab76f56a\`

The same body hash was confirmed in the original Clockwork source and the Stage 2 drill-patched source. Stages 5 and 6 did not target this function in their documented changes. The patcher therefore changes only this function and preserves unrelated Stage 6 source edits. If the function body differs, if the expected primary-only call is missing/duplicated, or if the helper already exists, it refuses to patch.

The actual Stage 6 source file was not present in the current working inputs. For that reason, the local full-file compile proof below uses the Stage 2 patched source as a syntax/integration check only; it is **not** a combined Stage 6 binary. Apply the function-only patcher to the Stage 6 \`maps__clockwork_interior.gsc\` source, then compile that resulting file before considering it integrated.

## Validation

- Read-only AI targeting audit: 5/5 checks passed.
- New host tests: 11/11 passed.
- GSC Tool 1.6.2.364 synthetic helper fixture: compile, decompile and byte-identical recompile passed.
- Function-only patch applied to the Stage 2 source; the resulting full GSC compiled and decompiled/recompiled byte-identically. This proves syntax on the Stage 2 base, not a Stage 6 combined build.

## Runtime gates

A PS3/RPCS3 Campaign test is still required to establish that \`getenemyinfo()\` produces the expected aggro behavior for both native local players, that the engine accepts the secondary entity as a valid enemy target, and that the discovery encounter remains stable after player death/rebind. The nearest-player choice occurs once at wake-up; dynamic retargeting after players move is not implemented.

No retail FastFile was rebuilt or signed, and no PKG was created.
