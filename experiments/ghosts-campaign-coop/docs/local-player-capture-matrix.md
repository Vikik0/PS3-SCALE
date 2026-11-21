# Local-player comparison matrix

## Objective

Compare a verified two-controller offline mode with Campaign on the exact Ghosts PS3 executable. This is the evidence-gathering stage before writing a Campaign hook.

## Capture identity for every run

| Field | Required value |
|---|---|
| Game Title ID | Exact installed ID, not inferred from region alone |
| Title Update | Exact installed update; first target 1.16 |
| Executable SHA-256 | Hash of the decrypted ELF used for analysis |
| Firmware | CEX/DEX/HEN family and exact version |
| Runtime | Real PS3 or named emulator/version |
| Mode and map | Exact offline mode / Campaign mission |
| Controllers | Port 0 and port 1; record which is assigned to each player |
| Result | Booted, joined, loaded, controlled, died, respawned, restarted |

## Compare these runtime concepts

| Concept | What must be established | Why it matters |
|---|---|---|
| Local player count | Where the engine records active local players and when it changes | Campaign must retain two player slots |
| Mode split-screen flag | Value returned by `isSplitScreen()` / mode-level split-screen state | Confirms the engine entered its local split-screen mode |
| Per-player split-screen identity | Results of `isSplitscreenPlayer()` and `isSplitscreenPlayerPrimary()` for each player | Distinguishes primary and secondary local players |
| Client/entity identity | `clientid`, entity number and their relationship to the local-player slot | Prevents confusing a spawned actor with a real local client |
| Controller assignment | Port-to-local-slot mapping and command generation | Player 2 must not steal Logan's input |
| Client state | Per-slot client state and lifecycle flags; compare against the reported `G_Client` base/stride | A second entity alone is not a second client |
| Entity array | Entity index and `G_Entity` base/stride | Confirms whether the player has a real world entity and how it relates to its client slot |
| Script VM state | `scrVmPub_t` context and current entity when split-screen builtins are invoked | Prevents unsafe direct calls to script-command handlers |
| Player state | Player-state address/ownership per local slot | Health, weapons and movement must remain independent |
| Player entity | Entity number and client pointer for each local player | AI and game logic need a real player target |
| Camera | Camera/refdef owner, local-client index and view origin | Each player needs an independent view |
| Viewport | Rectangle and split layout for each view | Both players must be visible |
| HUD | HUD local-client index and prompt ownership | UI must not overlap or respond to the wrong player |
| Input command | User-command buffer/sequence per local slot | Controls must be isolated and frame-correct |
| Sound/effects | Listener and local-client effect state | Prevent duplicated or misdirected audio/effects |
| Lifecycle | Spawn, death, respawn, checkpoint and transition behavior | Campaign must survive its scripted state changes |

## Minimum capture points

1. Main menu with controller 1 only.
2. Offline local-mode lobby before controller 2 joins.
3. Same lobby after controller 2 joins.
4. First playable frame with both players.
5. Each player moving/aiming independently.
6. One player down/dead while the other remains active.
7. Respawn or checkpoint reload.
8. Leave the local mode and return to menu.
9. Start a Campaign mission with controller 1 only.
10. Compare Campaign startup state to the local-mode state.

## Evidence format

For each point, record address, value, type, source (static reference or runtime observation), confidence and the exact moment captured. Keep raw memory dumps and game files local. Commit only a concise table of addresses/findings that are safe to publish, plus the executable hash and the analysis method.

Do not mark a value as a player slot merely because it changes when a controller joins. Confirm it through at least two independent observations, such as controller input plus player-state movement, and compare it against the same value in Campaign.

## Current status

This matrix is a capture plan, not a completed runtime observation. No PS3 memory capture or Campaign second-player initialization has been performed in CI.
