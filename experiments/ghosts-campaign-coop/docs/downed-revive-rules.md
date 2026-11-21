# Campaign downed / revive rules

This module implements the co-op failure and revive policy requested for the Campaign prototype. It is a deterministic, host-authoritative rules layer; it does not yet hook Ghosts' actual health, animation, movement, HUD, or checkpoint functions.

## Intended in-game behavior

- When either player reaches the game's downed threshold, only that player enters DOWNED. The mission continues while the partner is alive.
- A downed player is intended to crawl and use a sidearm, following the feel of Ghosts Extinction. These two presentation/gameplay hooks still require Campaign actor integration.
- The living partner can revive the downed player by holding the revive input while in range and with line of sight.
- Releasing the input, leaving range, losing line of sight, or the reviver becoming downed cancels the current revive progress.
- A successful revive restores the player at the configured health percentage.
- If both players are downed at once, the mission fails immediately.
- If a downed player's bleedout timer expires before a revive completes, the mission fails.
- Mission failure is sent to the Campaign adapter, which must invoke the game's real checkpoint/mission restart path. The rules module does not fake a restart by reloading the whole process.

## Defaults (tunable)

| Setting | Default |
|---|---:|
| Bleedout window | 30 seconds |
| Hold-to-revive duration | 5 seconds |
| Health after revive | 40% |
| Revives per mission | Unlimited; no custom cap is imposed |

These are initial project defaults, not a claim that the original Extinction mode uses identical timings. They can be changed in the build profile after playtesting.

## Host authority and network payload

The host console is the sole authority for downed/revive/mission-failure decisions. The 24-byte `GMS`-tagged, versioned snapshot can be sent as the opaque payload inside an authenticated GHC2 packet. It contains mission phase, failure reason, revision, both players' states and health, revive progress, and remaining bleedout time.

The remote client displays the received state; it must not independently trigger mission restart. The PS3 codec now exposes a combined authenticate-and-replay-check function backed by a wrap-safe 64-packet sequence window. Use the packet sequence to reject duplicate/out-of-order datagrams; use the mission revision to reject snapshots from an older mission state. Equal revisions remain valid for newer countdown/progress snapshots.

## Game adapter contract still required

1. Detect the real Campaign damage/death threshold and call `ghc_mission_player_downed`.
2. Put only the downed actor into the downed animation/movement state and enable the intended sidearm behavior.
3. Calculate revive range and line-of-sight from the real Campaign world and call `ghc_mission_update_revive` while the button is held.
4. Apply the configured revive health and restore the actor's normal movement/weapon state on `GHC_RULE_PLAYER_REVIVED`.
5. On `GHC_RULE_MISSION_FAILED`, invoke the actual Campaign checkpoint restart routine once, on the host, and replicate the resulting checkpoint transition.
6. Keep Logan's camera/input and all story NPCs, including Hesh, isolated from the second player actor.

Until those game-specific hooks are verified against the exact TU 1.16 executable and a real PS3, this is a tested rules component, not proof of playable Campaign co-op.
