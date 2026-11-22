# Phase 23 — Clockwork asset intake and Stage 5/6 reconstruction

## 1. Uploaded mission assets

The uploaded 'clockwork(1).ff' is 85,855,599 bytes and has SHA-256 'a2ac70c7349047f2eded5f45a5e46ff8278689839fe1655106263520fe639783', exactly matching the previously audited 'clockwork.ff'.

The read-only parser validated 2,048 outer 64 KiB blocks and produced a 128 MiB outer zone plus a 134,573,892-byte inner-inflated zone. The outer stream later reached an invalid raw-DEFLATE distance and left 16,557,959 bytes uninterpreted. Those trailing bytes were not treated as valid zone data.

The validated asset pool contains 4,153 entries and 1,999 script strings:

- 109 ScriptFile entries (type '0x29'), all 109 exported as compiled-GSC candidates.
- 55 RawFile entries (type '0x28'), 54 rawfiles plus one source file exported.
- One MapEnts entry (type '0x14'), pool index 4059, pointer '0xFFFFFFFF'.
- One 'col_map' entry (type '0x0F').
- The only map-like path string found is 'maps/clockwork.d3dbsp'.

'maps/clockwork_interior.gscbin' is 21,237 bytes. GSC Tool 1.6.2.364 decompiles it to 91,543 bytes with SHA-256 '7fc0f02476d5c9ebbe7c28e68abe762a26a976a28587228c5027d1a5c543e1f1', byte-identical to the previously audited retail source.

## 2. Additional FastFiles

| File | Blocks | Pool entries | ScriptFiles | MapEnts | Observed asset classes |
|---|---:|---:|---:|---:|---|
| 'clockwork_start_tr.ff' | 221 | 245 | 0 | 0 | 199 xanim, 23 xmodel, 21 techset, 1 rawfile |
| 'clockwork_end_tr.ff' | 214 | 315 | 0 | 0 | 282 xanim, 15 xmodel, 16 techset, 1 rawfile |
| 'por_clockwork.ff' | 17 | 2,380 | 0 | 0 | 2,101 sound, 276 localize, 1 stringtable, 1 rawfile |
| 'sna_clockwork.ff' | 17 | 2,380 | 0 | 0 | 2,101 sound, 276 localize, 1 stringtable, 1 rawfile |
| 'fra_clockwork.ff' | 17 | 2,380 | 0 | 0 | 2,101 sound, 276 localize, 1 stringtable, 1 rawfile |
| 'eng_clockwork.ff' | 16 | 2,380 | 0 | 0 | 2,101 sound, 276 localize, 1 stringtable, 1 rawfile |

These are useful presentation, animation, audio and localization assets, but they do not add another Clockwork ScriptFile or MapEnts entry.

## 3. Bink media

FFprobe recognizes all five uploaded videos as Bink:

- 'mission_preview_clockwork.bik': 0.50 s, 1280×720, 30 fps.
- 'clockwork_load.bik': 38.17 s, 1280×720, 29.97 fps, multiple Bink audio streams.
- 'clockwork_vault_drill.bik': 8.61 s, 1024×512, 29.97 fps.
- 'clockwork_vault_scan.bik': 3.97 s, 1024×512, 29.97 fps.
- 'clockwork_vault_still.bik': 1.03 s, 1024×512, 29.97 fps.

The interior script invokes 'cinematicingameloopresident("clockwork_vault_scan")' and 'cinematicingame("clockwork_vault_drill")'. These are mission-level cinematic events. Do not start duplicate copies per player until split-screen viewport behavior is verified in a live session.

## 4. MapEnts remains unresolved

The fully inner-inflated zone was scanned using the PS3 MapEnts candidate logic:

- Script-string count: 1,999.
- Valid inline entity-string candidates: 0.
- MapEnts pool record: index 4059, type '0x14', pointer '0xFFFFFFFF'.

None of the uploads is a standalone '.d3dbsp' or '.ents' file. The only map-like path string in the FastFile is 'maps/clockwork.d3dbsp'; the previously suggested 'maps/clockwork_interior.d3dbsp' name was not found. The exact class/targetname fields for 'drill_spot1' and 'drill_spot2' remain unverified.

## 5. Reconstructing the missing Stage 5/6 source

The historical merged Stage 5/6 'maps__clockwork_interior.gsc' source and patch diff were not retained in the available files. The repository does, however, document the exact intended behavior in:

- 'docs/phase18-clockwork-vault-blast.md' — Stage 5 shared vault blast and artillery rumble.
- 'docs/phase19-clockwork-interior-player-sync.md' — Stage 6 interior loadout/NVG, vault grenade ownership, hints, CQB movement and dog-attack notification.

I reconstructed those documented behaviors on top of the pinned Stage 2 source ('d2411ee6…'). This is a new, fail-closed reconstruction, not a claim to have recovered the historical Stage 6 file byte-for-byte.

### Stage 5 reconstruction

- Adds 'clockwork_apply_vault_blast(player, blast_origin)'.
- Clamps 'health - 5' damage to a minimum of zero.
- Applies damage, shellshock, prone stance and push to the explicit player.
- Clears the push after 0.2 seconds only if that same player is still alive.
- Routes the blast and artillery rumble to Logan and a validated native secondary player.

### Stage 6 reconstruction

- Routes the CZ805 and NVG setup to the validated secondary player.
- Sends vault NVG hints to both players and waits for both weapon-switch states.
- Saves Player 2's grenade counts and exact entity handle before the vault sequence.
- Restores those counts only if the current secondary alias is still the exact saved entity.
- Enables sprint/jump for the secondary player in CQB.
- Sends the dog-attack override notification to the secondary player.

A single 'clockwork_get_secondary_player()' helper now validates 'issplitscreen()', 'isplayer()', 'isalive()', 'issplitscreenplayer()', and '!issplitscreenplayerprimary()' before any Stage 5/6 secondary-player operation.

## 6. Stage 8 applied after the reconstruction

The Stage 8 AI-target patch was applied to the reconstructed Stage 6 source. The AI helper uses the same native-secondary identity checks and selects the nearest valid local player once when each discovery AI wakes up. It does not duplicate shared ally-to-enemy combat assignment, scripted dialogue, or audio-distance logic.

## 7. Local build validation

| Build | Source bytes | GSCBIN bytes | GSC Tool result | Round-trip |
|---|---:|---:|---|---|
| Reconstructed Stage 5 | 101,296 | 23,126 | Compile/decompile/recompile passed | Byte-identical |
| Reconstructed Stage 6 | 104,487 | 23,595 | Compile/decompile/recompile passed | Byte-identical |
| Reconstructed Stage 8 combined | 105,568 | 23,823 | Compile/decompile/recompile passed | Byte-identical |

Additional checks:

- Stage 5/6 source audit: 13/13 passed.
- Stage 5/6 patcher host tests: 7/7 passed.
- Stage 8 host tests: 11/11 passed.
- Original uploaded-source AI baseline audit: 5/5 passed.
- Updated Stage 8 synthetic GSC fixture: 320 bytes; compile/decompile/recompile byte-identical.

The older Phase 18/19 notes recorded different GSCBIN sizes (23,168 and 23,919 bytes). Because their full source and patch diffs were not retained, the reconstruction is behavior-based and must not be described as the exact historical Stage 5/6 binary.

## 8. Remaining gates

No live PS3/RPCS3 test was performed. Native Campaign Player 2 creation, independent camera/HUD, real AI aggro, the exact drill MapEnts triggers, and cinematic viewport behavior remain unverified. No retail FastFile was rebuilt or signed, and no PKG was created.
