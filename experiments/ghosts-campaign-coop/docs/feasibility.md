# Feasibility assessment — Call of Duty: Ghosts PS3 Campaign Co-op

## Current verdict

**The online lobby/relay layer is feasible to prototype. Full two-player Campaign co-op remains unproven and high-risk.** The existence of a working room server does not prove the game can safely create and replicate a second human-controlled actor in Campaign.

## Initial target

- Console: PS3 CEX CFW 4.93.
- Game update: Title Update 1.16.
- Initial target IDs: see compatibility-matrix.md.
- Player 1: original Logan player and story authority.
- Player 2: separate Multiplayer-style soldier actor.
- Hesh and all story NPCs remain untouched.

The regional title IDs are registered for future build selection, but no region has been hardware-validated for this mod.

## What public research establishes

- Ghosts PS3 modding primarily uses SPRX plugins, modified/debug EBOOTs and RTM/RPC tools; it does not have the public patch_mp.ff/GSC workflow common in some older Call of Duty games.
- The final game update is identified as 1.16 in the community research consulted for this project.
- Known regional/distribution IDs are listed in compatibility-matrix.md.
- CEX and DEX use different executable/plugin variants in the Ghosts modding ecosystem; DEX-only EBOOTs must not be used on CEX.
- Extinction is the game's existing co-op mode. Campaign is not documented as a two-player mode.

## What is not established

No public source located in this initial research demonstrates:

- A Campaign player-spawn hook that can create a second network-controlled actor.
- A safe method to keep Logan's original story logic intact while adding a second player actor.
- A complete authoritative replication path for player movement, weapons, damage, AI targeting, vehicles, scripted sequences, checkpoints and mission transitions.
- Compatibility across all Campaign missions or all regional game builds.
- A PSN authentication integration for a third-party private lobby service.

## Main technical risk

Campaign scripts may assume exactly one local player and bind mission triggers, camera logic, HUD, cinematics, checkpoints, weapon state and scripted actions to that player. Adding a second actor is not equivalent to adding a second lobby member. Reusing an Extinction/Multiplayer player model alone would not solve Campaign logic or network replication.

## Gate before full development

A local, offline proof of concept must show all of the following in one selected Campaign mission before investing in all missions:

1. Logan remains the original player and retains original Campaign progression.
2. A separate Multiplayer-style soldier actor is created without replacing Hesh or another story NPC.
3. The second actor can move and animate without corrupting Logan's camera/input.
4. The actor survives a checkpoint/reload and mission transition test, or is cleanly despawned/recreated.
5. No crash occurs during a defined 20-minute test and the game's original mission triggers still complete.

Only after this gate should online state replication be connected to the second actor.
