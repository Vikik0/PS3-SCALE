# IW6 native split-screen and Extinction reference

## Source and confidence

A public IW6 GSC script dump is available at [mjkzy/iw6dev-gsc-dump](https://github.com/mjkzy/iw6dev-gsc-dump), reviewed at commit `ae9886e1fa8e8d0a80e501a5f4cc6f17ab8563f7`.

The dump contains Multiplayer and Extinction scripts. It does **not** contain a Campaign script tree, and its exact platform/title-update provenance is not pinned in the repository. Treat it as an IW6 script-level reference, not as a verified PS3 TU 1.16 Campaign implementation. No script code is copied into the Ghosts PRX.

## Confirmed script-level split-screen primitives

1. `maps/mp/alien/_globallogic.gsc` sets `level.splitscreen = isSplitScreen()` during mode initialization ([source](https://github.com/mjkzy/iw6dev-gsc-dump/blob/ae9886e1fa8e8d0a80e501a5f4cc6f17ab8563f7/maps/mp/alien/_globallogic.gsc#L5-L12)).
2. `maps/mp/alien/_alien_matchdata.gsc` calls `set_split_screen()` on player connection and records `self isSplitscreenPlayer()` per `clientid` ([source](https://github.com/mjkzy/iw6dev-gsc-dump/blob/ae9886e1fa8e8d0a80e501a5f4cc6f17ab8563f7/maps/mp/alien/_alien_matchdata.gsc#L64-L122)).
3. `maps/mp/alien/_music_and_dialog.gsc` distinguishes the primary local split-screen player from the secondary player when starting spawn music ([source](https://github.com/mjkzy/iw6dev-gsc-dump/blob/ae9886e1fa8e8d0a80e501a5f4cc6f17ab8563f7/maps/mp/alien/_music_and_dialog.gsc#L72-L86)).
4. `maps/mp/gametypes/_playerlogic.gsc` adds connected player entities to `level.players`, removes them on disconnect, and can resolve an entity from a client number ([source](https://github.com/mjkzy/iw6dev-gsc-dump/blob/ae9886e1fa8e8d0a80e501a5f4cc6f17ab8563f7/maps/mp/gametypes/_playerlogic.gsc#L1058-L1074), [connect/disconnect lifecycle](https://github.com/mjkzy/iw6dev-gsc-dump/blob/ae9886e1fa8e8d0a80e501a5f4cc6f17ab8563f7/maps/mp/gametypes/_playerlogic.gsc#L1378-L1404), [player registration](https://github.com/mjkzy/iw6dev-gsc-dump/blob/ae9886e1fa8e8d0a80e501a5f4cc6f17ab8563f7/maps/mp/gametypes/_playerlogic.gsc#L1698-L1714)).

### What this establishes

The IW6 script layer exposes at least three distinct concepts:

- mode-level split-screen state: `isSplitScreen()`;
- per-player split-screen membership: `isSplitscreenPlayer()`;
- primary/secondary local-player identity: `isSplitscreenPlayerPrimary()`.

That is stronger evidence than a generic spawned soldier entity: Ghosts' IW6 script layer has native concepts for a local split-screen session and for identifying each player. It does not, by itself, reveal the underlying PS3 client-slot initialization, controller routing, renderer state, or whether Campaign can enter the same path.

## Extinction down/revive reference

The public Extinction script uses these values:

| Behavior | Script value |
|---|---:|
| Normal bleedout | 35 seconds |
| Normal teammate revive hold | 5 seconds |
| Spectator/dogtag revive hold | 6 seconds |

Sources: constants at the top of [`_laststand.gsc`](https://github.com/mjkzy/iw6dev-gsc-dump/blob/ae9886e1fa8e8d0a80e501a5f4cc6f17ab8563f7/maps/mp/alien/_laststand.gsc#L1-L12), normal revive call at [line 269](https://github.com/mjkzy/iw6dev-gsc-dump/blob/ae9886e1fa8e8d0a80e501a5f4cc6f17ab8563f7/maps/mp/alien/_laststand.gsc#L240-L272), and spectator revive at [line 310](https://github.com/mjkzy/iw6dev-gsc-dump/blob/ae9886e1fa8e8d0a80e501a5f4cc6f17ab8563f7/maps/mp/alien/_laststand.gsc#L272-L320).

The cooperative game-over rule checks whether every other player is already in last stand ([source](https://github.com/mjkzy/iw6dev-gsc-dump/blob/ae9886e1fa8e8d0a80e501a5f4cc6f17ab8563f7/maps/mp/alien/_laststand.gsc#L621-L657)). The revive hold continues only while the game is active, the reviver is alive, the use button remains held, and the reviver is not in last stand ([source](https://github.com/mjkzy/iw6dev-gsc-dump/blob/ae9886e1fa8e8d0a80e501a5f4cc6f17ab8563f7/maps/mp/alien/_laststand.gsc#L766-L830)).

The current Campaign rules retain the agreed prototype defaults (30-second bleedout, 5-second hold and 40% health after revive). The Extinction values are reference data only; they do not silently override the Campaign design.

## Engineering consequence for Campaign

The next reverse-engineering target is no longer “invent a second player entity.” It is to locate and verify the engine-side paths behind these script concepts in the exact PS3 EBOOT:

1. how `isSplitScreen()` obtains mode-level state;
2. how `isSplitscreenPlayer()` and `isSplitscreenPlayerPrimary()` resolve a player/client slot;
3. how `clientid`, entity number, local player slot and controller port relate;
4. how the secondary player's camera, viewport, HUD and user-command stream are initialized;
5. whether Campaign startup can preserve or initialize the same local-client slots.

The static ELF/OPD inspector and Ghidra seeding helper are the first analysis tools for this work. A runtime observation on the target PS3 is still required before any address becomes an approved hook.


## Detailed player lifecycle pass (reviewed 2026-11-21)

The following additional paths were inspected directly in the public dump:

- In _playerlogic.gsc, Callback_PlayerConnect() emits the level "connected" event before the rest of the player's initialization. The source comment explicitly says this event is more reliable than the older "connecting" event when two players arrive in the same frame. It assigns a persistent player clientid from the game client-id counter or the lowest available slot, and logs the engine entity number. References: [connect callback](https://github.com/mjkzy/iw6dev-gsc-dump/blob/ae9886e1fa8e8d0a80e501a5f4cc6f17ab8563f7/maps/mp/gametypes/_playerlogic.gsc#L1539-L1560), [client identity](https://github.com/mjkzy/iw6dev-gsc-dump/blob/ae9886e1fa8e8d0a80e501a5f4cc6f17ab8563f7/maps/mp/gametypes/_playerlogic.gsc#L1570-L1603).
- The player object is appended to level.players only after waittillframeend, allowing mode-specific connected listeners to initialize it first. The same file removes the object from level.players and participant/character arrays during disconnect. References: [registration](https://github.com/mjkzy/iw6dev-gsc-dump/blob/ae9886e1fa8e8d0a80e501a5f4cc6f17ab8563f7/maps/mp/gametypes/_playerlogic.gsc#L1698-L1714), [disconnect cleanup](https://github.com/mjkzy/iw6dev-gsc-dump/blob/ae9886e1fa8e8d0a80e501a5f4cc6f17ab8563f7/maps/mp/gametypes/_playerlogic.gsc#L1328-L1404).
- Extinction's onPlayerConnect() waits for each connected event, filters AI, then initializes match data, persistence, HUD, score, progression, achievements, unlocks, health regeneration and mode-specific threads. Reference: [aliens.gsc](https://github.com/mjkzy/iw6dev-gsc-dump/blob/ae9886e1fa8e8d0a80e501a5f4cc6f17ab8563f7/maps/mp/gametypes/aliens.gsc#L625-L730).
- _alien_matchdata.gsc records the split-screen boolean per player using that player's clientid. Reference: [_alien_matchdata.gsc](https://github.com/mjkzy/iw6dev-gsc-dump/blob/ae9886e1fa8e8d0a80e501a5f4cc6f17ab8563f7/maps/mp/alien/_alien_matchdata.gsc#L71-L145).
- The shared spawnPlayer(fauxSpawn) function is a respawn routine for an existing player object. It must not be mistaken for a way to create a new native local client. Reference: [_playerlogic.gsc](https://github.com/mjkzy/iw6dev-gsc-dump/blob/ae9886e1fa8e8d0a80e501a5f4cc6f17ab8563f7/maps/mp/gametypes/_playerlogic.gsc#L609-L760).
- Extinction branches its downed flow on isPlayingSolo(): solo uses self-revive, while co-op waits for another player to revive the downed player. The co-op game-over condition checks whether every other player is in last stand. References: [_laststand.gsc](https://github.com/mjkzy/iw6dev-gsc-dump/blob/ae9886e1fa8e8d0a80e501a5f4cc6f17ab8563f7/maps/mp/alien/_laststand.gsc#L240-L269), [co-op failure condition](https://github.com/mjkzy/iw6dev-gsc-dump/blob/ae9886e1fa8e8d0a80e501a5f4cc6f17ab8563f7/maps/mp/alien/_laststand.gsc#L620-L657), [revive hold](https://github.com/mjkzy/iw6dev-gsc-dump/blob/ae9886e1fa8e8d0a80e501a5f4cc6f17ab8563f7/maps/mp/alien/_laststand.gsc#L760-L830).

These are script-level findings only. They do not identify the PPU-side routine that allocates local client 1, nor prove that Campaign startup can use it.