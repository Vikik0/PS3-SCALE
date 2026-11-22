# Ghosts TU 1.16 patch FastFile analysis

Inputs inspected: user-supplied patch_common.ff and patch_common_mp.ff.

| File | Bytes | SHA-256 | Outer blocks | Decompressed zone bytes |
|---|---:|---|---:|---:|
| patch_common.ff | 144912 | 60c30ee31a70e541eca118fe0401e153a35a8735924ae0cb8b041f82c9bf1f5e | 1 | 65536 |
| patch_common_mp.ff | 646048 | 8ace37d43a053ccf3582e862bc479de7c2a6f1196728793e2b843e7e5cdb7120 | 17 | 1114112 |

## Extraction results

- patch_common.ff: 2 ScriptFiles extracted and decompiled: maps/_load.gsc and maps/_intelligence.gsc.
- patch_common_mp.ff: 119 ScriptFiles extracted and decompiled, plus all 7 RawFiles. Its asset pool lists 121 ScriptFiles; two records did not have a matching local payload in this patch file and remain unresolved.
- patch_common_mp asset pool: 2 netconststrings, 121 scriptfiles, 7 rawfiles, 6 xanim, 1 techset, 25 materials, 25 images.
- GSC Tool rename mode resolved the numeric ScriptFile names before decompilation.

## Campaign findings

TU 1.16 maps/_load.gsc has 66 functions. Sixty-five function bodies match the base common 1.00 version; the main entry function differs in two small places: it initializes `hideHudFast` to `0`, and it calls `maps\_gameskill::setsize()` before `maps\_art::magicreloadwhenreachenemy()` rather than after it. The player-registration function `init_level_flags` remains unchanged: it collects existing player entities into level.playerpiggyback, assigns level.player to index 0, and assigns level.playengineeffects to index 1 when a second entity already exists. This is player registration, not native client creation.

TU 1.16 maps/_intelligence.gsc has 21 functions. Three functions differ from the base version: poker_table_spooked, upload_hint_cleanup, and hold_count. The changes add validity guards and adjust polling; they continue to reference level.player rather than adding a second-player path.

## Multiplayer split-screen reference

patch_common_mp contains maps/mp/gametypes/_playerlogic.gsc, maps/mp/gametypes/_spawnlogic.gsc, maps/mp/_utility.gsc, maps/mp/_load.gsc, maps/mp/gametypes/_hud_util.gsc, and maps/mp/gametypes/_music_and_dialog.gsc.

In _playerlogic, callback_playerconnect waits for the engine-provided player entity to receive begin, then registers it in level.playerpiggyback. initclientdvars calls the native issplitscreenplayer predicate to apply split-screen HUD/FOV values. callback_playerdisconnect removes the entity from the list. This confirms that the multiplayer scripts manage local clients after the engine has created them; they do not create a new local client from GSC.

## Campaign co-op boundary

The previously analysed shared campaign scripts include a second-player-aware loader, get_other_player helper, and partner HUD helpers. However, the Clockwork mission scripts still contain 644 direct level.player references across maps/clockwork* files and no direct issplitscreen or coop references. They need mission-level adaptation after the engine can provide a second player entity.

## Engineering status

The two supplied TU 1.16 patch FastFiles are fully analysed for their ScriptFile and RawFile payloads. No additional MultiMAN transfer is needed for this analysis stage. The remaining blocking question is how the Campaign executable creates and routes a second native local client. No installable Campaign co-op PKG or real-PS3 player-two test exists yet; do not infer that toggling the coop dvar is sufficient.

Local extractor and tests are packaged in the analysis ZIP; they are read-only and do not rebuild or modify retail FastFiles.