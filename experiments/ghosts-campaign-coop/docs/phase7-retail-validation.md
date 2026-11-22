# Phase 7 — BLUS31270 retail-file validation

Scope: read-only validation of user-supplied Call of Duty: Ghosts PS3 files. No retail FastFiles, game scripts, or other copyrighted assets are committed.

## FastFile results

| File | Size | SHA-256 | IWffS100 offset | Blocks | Outer zone | Inner-inflated zone | Script candidates | RawFiles |
|---|---:|---|---:|---:|---:|---:|---:|---:|
| common.ff | 29,796,135 | 5fc31ea915894566896cc5081d3688675ecde341d42e8c38b19ad37434ca4bcb | 0x3294 | 717 | 46,989,312 | 47,689,203 | 150 | 30 extracted; 31 pool entries |
| clockwork.ff | 85,855,599 | a2ac70c7349047f2eded5f45a5e46ff8278689839fe1655106263520fe639783 | 0x16644 | 2,048 | 134,217,728 | 134,573,892 | 109 | 54 raw + 1 source |
| patch_common.ff | 144,912 | 60c30ee31a70e541eca118fe0401e153a35a8735924ae0cb8b041f82c9bf1f5e | 0x24 | 1 | 65,536 | 73,515 | 2 | 0 |
| patch_common_mp.ff | 646,048 | 8ace37d43a053ccf3582e862bc479de7c2a6f1196728793e2b843e7e5cdb7120 | 0x24 | 17 | 1,114,112 | 1,405,030 | 119 | 7 |

Clockwork has its IWffS100 marker at 0x16644, beyond the old 64 KiB scan limit. The block stream expands to 128 MiB, followed by 16,557,959 uninterpreted bytes. That tail is hashed and reported, not appended to the zone. Writing this partial-zone result requires the explicit allow-unparsed-tail option.

The Clockwork asset pool has 4,153 records, including its final sentinel. Its MapEnts entry is type 0x14 at pool index 4059, but its pointer is 0xFFFFFFFF. This is not a direct zone offset. The MapEnts structure, entity strings, and spawn points remain undecoded; no player-spawn conclusion is drawn.

Clockwork has 109 ScriptFile and 55 RawFile pool entries. The exporter reconstructs 109 structurally validated GSC Tool candidates, 54 rawfiles, and one short source asset. The earlier Phase 2 report records 109/109 successful decompiles; Phase 7 revalidated lengths and wrapper construction but did not repeat the decompiler run.

patch_common_mp has 121 ScriptFile and 7 RawFile pool entries. The current scan exports 119 compiled-script candidates and all 7 RawFiles. Two ScriptFile entries remain unmatched and are an explicit follow-up.

## Tool changes

The FastFile inspector now scans the first 0x40000 bytes, accepts and reports the four opaque bytes observed after each raw-DEFLATE block, keeps those bytes separate from the retail end marker, and raises the default block limit to 4,096. It can emit both the outer-decompressed raw zone and the inner-inflated zone. A file with an uninterpreted tail is not written unless the caller explicitly allows it.

The GSC exporter now consumes the outer-decompressed raw zone. For long assets it validates the big-endian FastFile lengths, decompresses the stack buffer, reads bytecode after the stored compressed stack, and reconstructs the GSC Tool wrapper: GSC magic, three little-endian lengths, the zlib stack, and bytecode. The wrapper is round-trip checked. Long assets are called compiled-GSC candidates until GSC Tool accepts them. Short assets are validated and exported as RawFiles or source.

## TU 1.16 executable audit

The supplied update PARAM.SFO identifies TITLE_ID BLUS31270, APP_VER 01.16, and PS3_SYSTEM_VER 04.4000. The disc PARAM.SFO separately identifies APP_VER 01.00.

| Executable | Build | Size | SELF type | Key revision | Entry point | Encrypted segments |
|---|---|---:|---:|---:|---:|---|
| EBOOT.BIN | TU 1.16 | 41,488 | 0x8 | 28 | 0x20080 | 0–4 |
| default.self | TU 1.16 | 4,291,568 | 0x8 | 28 | 0x7E9508 | 0–4 |
| default_mp.self | TU 1.16 | 6,265,632 | 0x8 | 28 | 0xA75E38 | 0–4 |
| EBOOT.BIN | disc 1.00 | 73,704 | 0x4 | 28 | 0x20080 | 0–4 |
| default.self | disc 1.00 | 8,072,104 | 0x4 | 28 | 0x7A8F28 | 0–4 |
| default_mp.self | disc 1.00 | 9,195,256 | 0x4 | 28 | 0x8B9738 | 0–4 |

TU 1.16 Campaign LOAD segments begin at 0x10000 and 0x7E0000. Multiplayer segments begin at 0x10000 and 0xA60000. The SELF segments remain encrypted. No PPU instructions, hook addresses, TOC values, or native second-client creation routine have been established by this metadata audit.

## Bootstrap status

The latest GitHub Actions run 45017456223 completed successfully across the GSC compile, server tests, worker checks, PRX bootstrap, and protocol-header jobs. Its diagnostic bootstrap artifact contains ghosts_coop.prx, ghosts_coop.fake.sprx, ghosts_coop.elf, and the 12,498-byte import stub archive. It has no Campaign player-two hook, loader integration, second camera, HUD integration, or playable co-op flow. It is not a playable mod. The earlier GSC registry artifact had a manifest mismatch (401 bytes declared versus a 400-byte archived file). The committed manifest was corrected in commit 75239b93d2108b67cc626d3c43f84e83302b3e7f; CI run 45017456223 now emits a 400-byte GSC binary whose SHA-256 matches the manifest. It is included as a compile/round-trip-verified diagnostic script only; it has not been injected or runtime-tested and it does not create a second player.

## Next engineering gates

1. Decode Clockwork MapEnts and resolve its pool pointer or relocation path.
2. Obtain matching Campaign and Multiplayer runtime captures from the exact TU 1.16 executables. Compare controller slots, local-client identity, client/entity/player-state addresses, and camera contexts.
3. Prove a native second local-client path before wiring any frame or input hook.
4. Validate Logan as player one, an independent player-two soldier, Hesh unchanged as an NPC, and independent input, camera, HUD, and AI state.
5. Adapt Clockwork mission dependencies only after the engine-level player-two gate passes.

Current truth: retail-file parsing and executable identity are grounded in the user's supplied BLUS31270 files. A second Campaign local client is still not created or proven, and no playable co-op build exists.


## Reproducing the asset-pool result

After exporting the raw zone, run the independent pool inspector to reproduce the ScriptFile, RawFile and MapEnts counts:

~~~powershell
python tools/ps3_zone_pool_inspect.py "C:\Ghosts\analysis\clockwork.raw.zone" --report-json "C:\Ghosts\analysis\clockwork-pool.json" --records-csv "C:\Ghosts\analysis\clockwork-pool.csv"
~~~

The inspector locates the pool by scoring the next 32 bytes after the tag strings, because the retail trailer length varies. It reports the MapEnts pool record and its pointer verbatim; it does not interpret the pointer or decode entity data.


## PS3 MapEnts layout probe

A PS3-specific IW6 MapEnts reference was recovered in the IW4Studio MapConverter.IW6.PS3 research script. It documents a 0x80-byte MapEnts header, entity text at header + 0x80, numEntityChars at +8, SpawnList count at +0x70, and 0x20-byte SpawnPointEntityRecord entries. The script-string table uses the count at 0x28 and 32-bit slots beginning at 0x3C (zero means empty; 0xFFFFFFFF means the next inline string). These are layout references only; the map-specific hard-coded offsets in that script belong to mp_swamp and were not reused for Clockwork.

The new experimental ps3_zone_mapents_probe.py implements that 32-bit layout and validates the entity-string length, script-string indexes, and spawn-record sequence. It passes synthetic fixtures, but the fully inner-inflated Clockwork zone produced zero MapEnts entity-string candidates. The pool entry therefore remains unresolved and no spawn-point coordinates are claimed. The next input needed for this path may be a separate map-data asset or a runtime/dumped MapEnts region, not another guess at the mp_swamp offsets.
