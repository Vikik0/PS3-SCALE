# Clockwork Campaign co-op adaptation plan

## Scope

This plan uses the available Clockwork mission GSC sample only. It is a one-mission proof-of-concept plan, not a claim of full Campaign coverage. The native second local-player creation path is still a prerequisite; none of the mission adapters below can make a second player exist by itself.

## Core rule: do not globally replace level.player

The mission contains 644 direct references to level.player across 205 functions. That pointer is the primary story player. A blind replacement with a loop over both players would duplicate mission-wide events, double-trigger flags, replay one-shot dialogue, duplicate weapon grants, and make scripted animations race.

Use the read-only registry shim to obtain primary and secondary native player handles. Then classify each function as:

- **Player-scoped:** movement, stance, weapon, input, damage, camera, view effects, local HUD and controller prompts. These should accept a player handle.
- **Mission-scoped:** objective flags, enemy waves, one-time dialogue, autosaves, scripted world events and shared inventory counts. These should remain single shared state.
- **Shared interaction:** one world trigger that either local player may activate. It must identify the activating player, apply the item/action to that player, and update one shared mission counter exactly once.

## Clockwork high-risk function map

| Priority | Script/function | Existing behavior | Co-op adaptation requirement |
|---|---|---|---|
| P0 | maps__clockwork_interior.gsc::handle_drill_spot | One level.player owns the drill input, weapon checks, stance, controls, animation link, ammo changes and action-slot cleanup. | Make the active driller explicit. Permit one player to operate a drill spot at a time; bind the drill animation and controls only to that player. Keep the drill spot completion flag shared and atomic. |
| P0 | maps__clockwork_code.gsc::hold_count_check | Monitors one player's ADS/fire state and temporarily swaps weapons while enforcing hold-fire rules. | Track which player is performing the hold action. Do not let P2's fire/ADS input satisfy or cancel P1's hold state accidentally. |
| P0 | maps__clockwork_interior.gsc::new_trench_setup | Sequences vault setup, two drill spots, weapon transitions, scripted movement, damage, shellshock and autosave. | Keep world sequence and autosave shared; route movement/weapon/damage effects to the intended player. Decide whether the second player waits, assists or receives a synchronized animation during each one-player set piece. |
| P0 | maps__clockwork_intro.gsc::intro_door | Freezes and moves the primary player through the jeep-door interaction/cinematic. | Keep the story trigger shared but preserve independent control/camera state. P2 must not be left frozen or teleported to the wrong seat. |
| P1 | maps__clockwork_interior.gsc::handle_misc_drill_details | Reads ADS/attack and current weapon for the drill interaction and plays local feedback. | Route input and rumble/sound to the active driller; leave shared drill state on the mission controller. |
| P1 | maps__clockwork_exfil.gsc::in_spawnspectator | Controls exfil spawn sequence, player movement permissions and scripted jeep transition. | Define a two-player vehicle entry order and ensure both player entities remain valid through the transition. |
| P1 | maps__clockwork_intro.gsc::player_bob_scale_set | Applies stance, weapon and control changes during a scripted camera/animation segment. | Apply view/weapon/control changes per player, not to a shared alias. |
| P1 | maps__clockwork_defend.gsc::listen_for_use_proximity_duffle_bag | Grants a shockwave weapon to the primary player from a shared bag trigger and decrements a shared supply count. | Either player can activate the trigger; grant to the activator, protect the shared supply count from double activation, and update only that player's action slot. |
| P1 | maps__clockwork_defend.gsc::listen_for_use_multi_turret | Grants a thermobaric mine to the primary player and starts shared defend threads. | Grant to the activator and keep shared wave/defend flags idempotent. Verify two players can use separate turret/mine interactions without sharing weapon state. |
| P1 | maps__clockwork_exfil.gsc::new_l_pip_corner | Changes weapons, invulnerability, view angle and player linking for a scripted exfil/PIP sequence. | Separate player view/camera state and decide whether P2 gets a parallel camera or remains in a safe companion state during the PIP. |

## Shared-state and race-condition rules

1. **One-shot flags stay shared.** Drill completion, defend completion, exfil failure and autosave/checkpoint flags represent mission progress, not per-player state.
2. **Player actions are attributed.** A trigger callback must carry the actual activating player entity. Never infer the activator from level.player.
3. **Inventory stays per player; supply counts stay shared.** Weapon grants, ammo, offhand slots and weapon switching belong to the activating player. Duffle-bag stock and mission progression remain shared.
4. **Scripted movement must be explicit.** If a cutscene links a player to an animation rig, the code must name which local player is linked and must unlink/restore that same player on every exit path.
5. **Input subscriptions must be per player.** ADS/fire/use notifications must be bound to the correct player; one player's input cannot complete or cancel the other's hold action.
6. **Audio and HUD are local-player aware.** Use the TU 1.16 Multiplayer split-screen reference for viewport sizing and primary-only dialog/audio policy, but do not blindly copy Multiplayer behavior into Campaign.
7. **Checkpoint and restart restore both slots.** No second player may be duplicated, dropped or left with stale control/camera state after an autosave reload.

## Implementation sequence

### Gate A — native player slots

- Verify exactly two native split-screen player objects.
- Bind primary/secondary by issplitscreenplayerprimary(), not by guessed array order.
- Confirm controller 0/1 mapping, independent gclient, gentity, player-state, user-command and viewport state.
- Fail closed if any identity or state is missing.

### Gate B — one Clockwork interaction

Implement and test one safe interaction first: a single shockwave duffle-bag pickup. Prove either controller can activate it, only the activator receives the weapon, the shared stock decreases once, and the other player's state remains unchanged.

### Gate C — drill sequence

Adapt handle_drill_spot and handle_misc_drill_details around an explicit active-player parameter. Test both players taking turns at both drill spots, including weapon switching, ADS/fire, cancellation, animation unlink and checkpoint reload.

### Gate D — combat/defend and exfil

Add player-attributed pickup/turret use, enemy targeting of both players, independent scripted movement and two-player vehicle transition. Verify shared flags remain one-shot.

### Gate E — mission recovery and broader coverage

Add downed/revive, bleedout, both-down failure, checkpoint restart and mission transition. Only after Clockwork is stable should the same audit be run against the remaining Campaign mission FastFiles.

## Non-goals until the native gate passes

- Do not add a fake AI clone as Player 2.
- Do not change level.player globally.
- Do not patch or rebuild signed retail FastFiles.
- Do not claim full Campaign coverage from the Clockwork sample.
- Do not build an installable PKG until the native slot and mission hooks are verified on the exact TU 1.16 target.

## Phase 11 — native local-session reuse vertical slice

The first Campaign-side adapter is now at client/ps3/gsc/ghc_campaign_local_session.gsc.

- Reuse the engine-created local split-screen players from the native Multiplayer path.
- Refresh the Campaign player entity list from getentarray("player", "classname") for a bounded startup window.
- Identify primary and secondary with issplitscreenplayer() and issplitscreenplayerprimary(); never infer slot identity from entity-array order or match clientid.
- After validation only, bind level.player to the primary and level.playengineeffects to the secondary.
- Preserve Logan's original setup_combat() and apply its known starting loadout to Player 2 through ghc_campaign_apply_clockwork_player_two_loadout().

The helper is intended to run after the original Player 1 setup_combat() call. It does not spawn the native second client, and it is not yet loaded by the retail Campaign VM.

The first mission behavior to adapt after the native gate passes is the drill interaction. Its input, weapon checks, animation link, stance and control locks must belong to the player who activates the drill. Drill completion, objective flags and autosave remain one shared mission state. Only one player may own a drill interaction at a time; cancellation and every exit path must restore that same player's controls and animation link.

This vertical slice is intentionally smaller than a blanket 644-reference conversion. It proves the native identity bridge and one per-player gameplay interaction before broader mission refactoring.

## Phase 12 — GSC-side drill ownership adapter

The local-session bridge now exposes a two-spot GSC ownership contract in ghc_campaign_local_session.gsc:

- ghc_campaign_drill_begin(player, spot) accepts only one of the two validated native Campaign player entities, and only while the native pair is ready.
- ghc_campaign_drill_complete(player, spot) commits the shared spot-complete flag only for the player who owns that interaction.
- ghc_campaign_drill_cancel(player, spot) releases the lock without completing the spot.
- ghc_campaign_drill_abort_player(player) and ghc_campaign_drill_abort_slot(slot) release the owner lock on interruption/death/disconnect. Slot-based cleanup remains available even after the session-ready flag is dropped.
- The claim/finish critical sections contain no wait/yield. GSC's serialized script execution therefore prevents two button events from interleaving the owner check and assignment.

The original Clockwork handle_drill_spot and handle_drilling source files are not in the public repository. Do not patch them by line number from the old reference matrix. The helper is ready for a source-level adapter, but the retail mission function still needs to be wired to pass its actual activating player and the actual drill-spot index, then restore that same player's stance, weapon locks, animation link and action slot on every exit path.

The portable C ownership state machine remains the host-test oracle for these rules. It is not injected into the game and does not replace the GSC helper.

