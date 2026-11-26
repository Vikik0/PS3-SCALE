# Phase 118 — original mission trigger call-site inventory

## Source identity

- Original archive: Ghosts_Campaign_Missions_Phase31.zip
- Archive size: 1,530,334 bytes
- Archive SHA-256: ca4c0fb46a12b6179b406bc6f7ee5e10729c66ac7a7d2664bf9c3d3cfa14a77a

## Generic helper call sites

The supplied decompiled source contains 15 original calls to waittill_trigger. Every one still calls the legacy helper; none passes an explicit player entity to the new Player-Aware helper.

| Mission | File | Line | Caller | Helper |
|---|---|---:|---|---|
| Black Ice | maps/black_ice_ascend.gsc | 293 | ascend_logic | black_ice_util::waittill_trigger |
| Black Ice | maps/black_ice_command.gsc | 66 | magicreloadwhenreachenemy | black_ice_util::waittill_trigger |
| Black Ice | maps/black_ice_flarestack.gsc | 268 | event_player_turns_key | black_ice_util::waittill_trigger |
| Carrier | maps/carrier_deck_transition.gsc | 432 | osprey_control | player_scripted_anim_util::waittill_trigger |
| Carrier | maps/carrier_defend_sparrow.gsc | 139 | run_deck_transition | player_scripted_anim_util::waittill_trigger |
| Cornered | maps/cornered_building_entry.gsc | 346 | handle_building_entry | player_scripted_anim_util::waittill_trigger |
| Cornered | maps/cornered_building_entry.gsc | 671 | upload_virus_loop_anims | player_scripted_anim_util::waittill_trigger |
| Cornered | maps/cornered_building_entry.gsc | 766 | upload_virus_anims | player_scripted_anim_util::waittill_trigger |
| Cornered | maps/cornered_building_entry.gsc | 783 | upload_virus_anims | player_scripted_anim_util::waittill_trigger |
| Cornered | maps/cornered_building_entry.gsc | 2757 | handle_rappel_inverted | player_scripted_anim_util::waittill_trigger |
| Cornered | maps/cornered_destruct.gsc | 794 | hvt_office_anim_struct | player_scripted_anim_util::waittill_trigger |
| Cornered | maps/cornered_interior.gsc | 2398 | junction_elevator_control_doors_open | player_scripted_anim_util::waittill_trigger |
| Cornered | maps/cornered_intro.gsc | 1896 | zipline_mvmt_npc | player_scripted_anim_util::waittill_trigger |
| Cornered | maps/cornered_rappel.gsc | 131 | player_combat_rappel | player_scripted_anim_util::waittill_trigger |
| Cornered | maps/cornered_zipline_turret.gsc | 20 | player_handle_speed_for_knockdown | player_scripted_anim_util::waittill_trigger |

Source file hashes are pinned in reports/PHASE118_TRIGGER_CALLSITE_INVENTORY.json.

## Four critical shared interactions

The Phase 89 activator-lineage audit still defines the four mission gates:

1. Black Ice — event_player_turns_key, event ID 0. The legacy helper polls level.player; the console routine has no player parameter. It needs an activator-aware console path and a single owner for flare-stack flags and animation.
2. Carrier — cut_the_rope, event ID 1. This path uses notifyonplayercommand("cut", ...), not an entity trigger. Phase 117 adds the matching player-command adapter, but the rope watcher, per-player eligibility, animation and hint aggregation still need refactoring.
3. Cornered — gun_down_trigger_internal, event ID 2. The routine is started by a global flag and uses level.player for weapon snapshots; the shared ammo-counter dvar needs one coordinator.
4. Cornered — upload_virus_enter, event ID 3. Trigger, progress, exit and enter handlers hand off through global flags. The activator must be carried through the entire chain while the device, dynamic DOF and shared flags remain single-owner.

## Adapter coverage

- Black Ice helper prototype: explicit player-scoped aim/use checks.
- Carrier/Cornered helper prototype: explicit player context plus a trigger-event activator equality check.
- Bridge entity-trigger adapter: ghc_campaign_wait_for_owned_trigger.
- Bridge player-command adapter: ghc_campaign_wait_for_owned_player_command.

These are building blocks only. The 15 generic call sites and the four critical interaction chains remain unmigrated. Do not treat helper compilation as proof of working co-op.
