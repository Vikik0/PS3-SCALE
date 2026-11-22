# Ghosts PS3 FastFile and GSC extraction research

 **Scope:** Call of Duty: Ghosts / IW6, PS3 retail FastFiles only. This is a read-only research and extraction path for a user-owned game copy. It does not patch the game, rebuild a FastFile, bypass signature verification, or include retail assets.

## User file audit — BLUS31270 (2026-11-21)

The user's uploaded `PARAM(1).SFO` was parsed and confirms `TITLE_ID=BLUS31270`, `TITLE=Call of Duty Ghosts`, `APP_VER=01.00`, `VERSION=01.00`, `PS3_SYSTEM_VER=04.4600`, `CATEGORY=DG`. This is base-disc metadata, not proof of TU 1.16.

The accompanying screenshot shows the ISO path `PS3_GAME/USRDIR` and entries including `ui_mp.ff`, `ui_install.ff`, `ui.ff`, language folders and `soundfile*.pak`. It is a partial listing, so it does not prove that EBOOT or other FFs are absent.

**Path correction:** do not require a literal `USRDIR/zone` folder. PS3 Ghosts FastFiles are commonly in the `PS3_GAME/USRDIR` root, with localized assets under language folders. “Zone” also means the decompressed internal content of an FF. Inventory all `.ff` files recursively from USRDIR and preserve their original names. The base zone appears in recent PS3 research as `ghosts_ps3_common.ff`; older/alternate inventories may call the corresponding file `common.ff`. Determine the exact filename from this user's dump before choosing it for analysis.

The matching BLUS31270 TU 1.16 package is listed on Sony's CDN as `UP0002-BLUS31270_00-CALLDUTYGHOSTTU1-A0116-V0100-PE.pkg` (558,341,392 bytes): https://b0.ww.np.dl.playstation.net/tppkg/np/BLUS31270/BLUS31270_T16/35760cb2130fa142/UP0002-BLUS31270_00-CALLDUTYGHOSTTU1-A0116-V0100-PE.pkg. It is an update package, not the base game. Keep disc EBOOT and TU EBOOT separate and never substitute a different region's package.

A public BLUS31270 disc-repair index also records an IRD for the original disc and a small repair archive listing original PARAM.SFO/EBOOT.BIN; this is a verification lead, not a reason to replace the user's own dump. Prefer the user's own disc EBOOT and use the IRD only for disc-integrity checking. [BLUS31270 disc-repair index](https://archive.midnightchannel.net/SonyPS/PS3/Disc%20Repair/US/%5BBLUS31270%5D%20Call%20of%20Duty%20Ghosts/).


## Findings that change the previous plan

The PS3 retail FastFile format is now documented well enough to extract the outer zone and many inner assets. The earlier assumption that the data following IWffS100 was encrypted was superseded by the later May 2026 Part 2 research: the 112 KiB LO region is metadata, and the actual raw-deflate stream begins at A + 0x20000, where A is the location of IWffS100.

- Outer magic: IWff0100
- Version: big-endian 0x0000022E
- Inner marker: IWffS100 (capital S)
- Patch files commonly have A = 0x24; a documented base common.ff sample has A = 0x3294
- Deflate stream: repeated [u16 big-endian compressed length][raw-deflate payload]; each payload expands to 64 KiB
- The resulting zone contains inline per-asset zlib streams. Short headers are used for rawfiles; long headers are used for scriptfile/mptype/aitype assets.
- Retail files are signed. Re-signing modified FastFiles requires Infinity Ward's RSA-2048 private key, so this project will extract and analyze only; it will not write modified retail .ff files.

References: [CoD-Research FastFiles](https://github.com/primetime43/CoD-Research/blob/main/Ghosts-COD10/FastFiles/FastFiles.md), [CoD-Research Zone format](https://github.com/primetime43/CoD-Research/blob/main/Ghosts-COD10/FastFiles/Zone.md), [ADNMRTNS Part 2 — format cracked](https://www.adnmrtns.ca/blog/ghosts-fastfile-research-part2).

## Public tools checked

1. **CoD-FF-Tools dev branch** — contains Ghosts PS3 support in FastFileProcessor.TryDecompressGhosts, GhostsZoneLayout, and the editor's GhostsAssetWalker. The inspected dev head is 2cffc71b8275334527661b6aada744c9929a4c90 (2026-06-08). Its ffcli decompress path can produce a zone, and the editor can display located Ghosts assets. Important: the public main / v4.0.0 release README still lists CoD4, WaW and MW2, not Ghosts. Do not assume the v4.0.0 release binary has the dev-branch Ghosts support. Also, the dev branch's generic ffcli list/extract path still calls the older RawFileScanner; it is not yet a verified Ghosts GSC exporter.
2. **GSC Tool** — supports IW6 on PS3 and has an explicit ps3 system option. PS3/Xbox 360 GSC binary length fields are little-endian. After extracting a compiled GSC magic asset, use gsc-tool -m decomp -g iw6 -s ps3 <file.gscbin> to decompile it. [Repository and usage](https://github.com/xensik/gsc-tool).
3. **open-fastfile / x64-zt** — useful IW6 research, but their Ghosts asset paths rely on x64/Durango layouts or development symbols. Those 64-bit structures are not interchangeable with the PS3 PPU zone layout. Do not use their x64 offsets or structures in this PS3 build.
4. **Public IW6 script dump** — mjkzy/iw6dev-gsc-dump exposes Multiplayer/Extinction split-screen and revive script logic, but it is not pinned to PS3 TU 1.16 and does not include the Campaign script tree. It remains reference material, not proof of PS3 Campaign behavior.

## New local tools in this project

### 1. tools/ps3_fastfile_inspect.py

Read-only PS3 Ghosts .ff inspector and zone extractor. It:

- validates IWff0100 and version 0x22E;
- locates IWffS100 in the first 0x40000 bytes (the supplied Clockwork marker is at 0x16644) and applies A + 0x20000;
- expands 64 KiB raw-deflate blocks and accepts/reports the four opaque bytes observed after each compressed block;
- recognizes short and long IW6 asset headers and expands valid inline zlib streams;
- records file/zone sizes, block count, inner stream count, SHA-256 and any trailing bytes;
- refuses to overwrite the source file; it can emit both the outer-decompressed raw zone and the inner-inflated zone, and refuses to write an unparsed tail unless --allow-unparsed-tail is explicit.

Example:

~~~powershell
python tools/ps3_fastfile_inspect.py "C:\Ghosts\common.ff" --output-zone "C:\Ghosts\analysis\common.zone" --report-json "C:\Ghosts\analysis\common-report.json"
~~~

The known retail writer terminates the block stream with the two-byte marker 00 01 followed by exactly four zero padding bytes. The inspector recognizes that exact marker and reports the four padding bytes. Arbitrary trailing data and truncated blocks are not accepted as complete.

### 2. tools/ps3_zone_script_export.py

Scans the outer-decompressed raw .zone (inline asset zlib streams remain compressed). It recognizes short and long IW6 headers, rebuilds the GSC Tool ScriptFile wrapper from long assets, exports short assets as RawFiles or source, validates lengths and safe paths, and writes a JSON inventory.

~~~powershell
python tools/ps3_zone_script_export.py "C:\Ghosts\analysis\common.zone" "C:\Ghosts\analysis\scripts" --report-json "C:\Ghosts\analysis\scripts.json"
~~~

After decompiling compiled assets with GSC Tool, inventory likely player/session and Campaign hooks:

~~~powershell
python tools/gsc_campaign_audit.py "C:\Ghosts\analysis\scripts" --report-json "C:\Ghosts\analysis\gsc-audit.json"
~~~

The exporter deliberately does not classify assets by a hard-coded type ID. Public references disagree on some IW6 asset-type IDs, and names plus header shape plus GSC magic are safer for this first-pass script inventory. Embedded rawfiles inside mptype map structures are not yet extracted; that requires a separate MapEnts parser.

## Test status and limitations

Both tools have synthetic-fixture tests for retail block trailers, marker offsets beyond 64 KiB, raw/inner zone output, GSC wrapper reconstruction, malformed inputs, and output path safety. Phase 7 also ran the tools against the user's exact BLUS31270 common.ff, clockwork.ff, patch_common.ff, and patch_common_mp.ff files. Counts, hashes, TU 1.16 executable metadata, and the remaining Clockwork tail/MapEnts limitations are recorded in docs/phase7-retail-validation.md.

The known multi-zone DLC files common_alien_dlc_updated_mp.ff and common_core_dlc_updated_mp.ff use a different container and are outside this parser's current scope. The normal PS3 retail patch/base variants are the target.

## Next reverse-engineering pass

1. Inventory all .ff files from the user's exact PS3 title ID and TU 1.16 installation; do not mix regions.
2. Extract the base common.ff and all Campaign/SP-related zones first, then patch zones.
3. Export all .gsc / .csc source assets and compiled GSC magic scriptfiles; decompile compiled files with GSC Tool's IW6 PS3 mode.
4. Search the resulting Campaign scripts for player-count assumptions, player connect/spawn lifecycle, isSplitScreen() / isSplitscreenPlayer() calls, player entity registration, checkpoints, revive/last-stand and mission transitions.
5. Run tools/gsc_campaign_audit.py over the decompiled scripts and review its line-numbered leads.
6. Compare those findings with the public Extinction/Multiplayer reference, then return to the exact PS3 EBOOT/PRX analysis to verify the engine-side local-client setup.

This moves script archaeology ahead without pretending that a second Campaign player has been created. The main blocker remains proving the native second local-client path in the exact PS3 executable.


## Cross-check against independent Ghosts tooling

The public CoD-FF-Tools dev branch has a Ghosts PS3-specific decompressor and zone walker. Its inspected dev commit is 2cffc71b8275334527661b6aada744c9929a4c90 (2026-06-08). Its Ghosts extraction how-to documents the same IWffS100 anchor and A + 0x20000 stream offset, and says the two-pass extractor was checked against four retail PS3 samples (one base zone and three patches).

Important implementation details cross-checked:
- The retail stream ends with the two-byte 0x0001 marker and four zero padding bytes; the reference decompressor stops on a zero/one-byte length field and does not treat the marker as a compressed block.
- The PS3 asset pool uses an eight-byte record with a big-endian type ID. The public PS3 enum places aitype at 0x24, mptype at 0x25, rawfile at 0x28, scriptfile at 0x29 and luafile at 0x32.
- Wrapped rawfile/scriptfile/mptype/aitype bodies use short or long headers. Luafiles have a separate flat 16-byte header and are not zlib-wrapped.
- The public asset walker currently pairs scriptfile/mptype/aitype records with located headers but does not export their bodies as script nodes. This remains a gap for our GSC extraction stage.

References: [Ghosts extraction how-to](https://github.com/primetime43/CoD-FF-Tools/blob/dev/docs/Ghosts_Extraction_Howto.md), [Ghosts PS3 asset enum](https://github.com/primetime43/CoD-FF-Tools/blob/dev/FastFileLib/GameDefinitions/GhostsDefinition.cs), [Ghosts zone layout](https://github.com/primetime43/CoD-FF-Tools/blob/dev/FastFileLib/GhostsZoneLayout.cs), [Ghosts asset walker](https://github.com/primetime43/CoD-FF-Tools/blob/dev/Call%20of%20Duty%20FastFile%20Editor/ZoneParsers/GhostsAssetWalker.cs).

The public GSC Tool dev branch supports IW6 on PS3 and defines the PS3 system as big-endian. Its README documents the GSC binary header and specifies that the three length fields in PS3/Xbox 360 .gscbin files are little-endian. The next step after extraction is to run the GSC Tool decompiler in IW6/PS3 mode. The bytecode symbol tables list these relevant VM builtin IDs:
- function 0x0131: isSplitScreen
- method 0x807A: getEntityNumber
- method 0x80E2: isSplitscreenPlayer
- method 0x80E3: isSplitscreenPlayerPrimary

These are GSC VM builtin identifiers, not PS3 PPU function addresses and must not be used as EBOOT hook addresses.

References: [GSC Tool README](https://github.com/xensik/gsc-tool/blob/dev/README.md), [IW6 PS3 function symbols](https://github.com/xensik/gsc-tool/blob/dev/src/gsc/engine/iw6_ps_func.cpp), [IW6 PS3 method symbols](https://github.com/xensik/gsc-tool/blob/dev/src/gsc/engine/iw6_ps_meth.cpp).

### Compiled GSC validation

The script exporter does not expect GSC magic to be present in the original FastFile asset body. It validates the long asset header, zlib-decompresses the stored stack buffer, reads bytecode immediately after the stored compressed stack, and constructs the GSC Tool wrapper (GSC NUL magic, three little-endian lengths, compressed stack, bytecode). It round-trip validates the wrapper before exporting it as a compiled-GSC candidate; GSC Tool decompilation remains the final confirmation.

### Windows batch decompilation

After exporting compiled GSC assets, use the helper to decompile every .gscbin/.cscbin with GSC Tool's IW6 PS3 backend:

~~~powershell
powershell -ExecutionPolicy Bypass -File tools\decompile_iw6_ps3_scripts.ps1 -GscToolPath "C:\Tools\gsc-tool.exe" -InputDirectory "C:\Ghosts\analysis\scripts" -OutputDirectory "C:\Ghosts\analysis\decompiled"
~~~

The helper copies the binary tree into the output directory before decompiling, so the original exported assets remain unchanged.

## Phase 7 — actual retail sample validation

The exact user-supplied BLUS31270 FastFiles and TU 1.16 executable set have now been parsed. See docs/phase7-retail-validation.md for file hashes and sizes, block counts, asset counts, SELF types and entry points. Key findings: Clockwork's marker is at 0x16644; its 2,048 valid blocks expand to a 128 MiB zone followed by 16,557,959 uninterpreted bytes. Its MapEnts pool entry is type 0x14 at index 4059 with pointer 0xFFFFFFFF, so spawn entities remain undecoded. The current extraction produces 109 compiled-GSC candidates, 54 RawFiles and one short source asset; the previous 109/109 decompile result was not repeated in this phase.

The per-block four-byte suffix is reported as opaque; its meaning is not established. The exact end marker remains separate: 00 01 followed by four zero padding bytes. No modified FastFile is written or signed.


### 3. tools/ps3_zone_pool_inspect.py

Reads the 8-byte asset-pool records from an outer-decompressed raw zone. It scans a 32-byte variable trailer after the tag strings, reports type counts, and calls out MapEnts entries without treating their pointer values as file offsets.

~~~powershell
python tools/ps3_zone_pool_inspect.py "C:\Ghosts\analysis\clockwork.raw.zone" --report-json "C:\Ghosts\analysis\clockwork-pool.json" --records-csv "C:\Ghosts\analysis\clockwork-pool.csv"
~~~


### 4. tools/ps3_zone_mapents_probe.py

Experimental PS3 MapEnts candidate scanner. It reads the inline script-string table, searches for the 0x80-byte PS3 MapEnts header followed by a length-matching entity string, and validates 0x20-byte spawn records when a spawn-list offset is resolvable. It does not apply map-specific offsets from other missions. Clockwork currently yields zero entity-string candidates, so no spawn coordinates are exported.

~~~powershell
python tools/ps3_zone_mapents_probe.py "C:\Ghosts\analysis\clockwork.zone" --report-json "C:\Ghosts\analysis\clockwork-mapents.json" --spawn-csv "C:\Ghosts\analysis\clockwork-spawns.csv"
~~~
