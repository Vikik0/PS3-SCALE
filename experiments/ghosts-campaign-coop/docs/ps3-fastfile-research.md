# Ghosts PS3 FastFile and GSC extraction research

**Scope:** Call of Duty: Ghosts / IW6, PS3 retail FastFiles only. This is a read-only research and extraction path for a user-owned game copy. It does not patch the game, rebuild a FastFile, bypass signature verification, or include retail assets.

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
- locates IWffS100 in the first 64 KiB and applies the documented A + 0x20000 rule;
- expands the 64 KiB raw-deflate blocks;
- recognizes short and long IW6 asset headers and expands valid inline zlib streams;
- records file/zone sizes, block count, inner stream count, SHA-256 and any trailing bytes;
- refuses to overwrite the source file and refuses to write an incomplete zone.

Example:

~~~powershell
python tools/ps3_fastfile_inspect.py "C:\Ghosts\common.ff" --output-zone "C:\Ghosts\analysis\common.zone" --report-json "C:\Ghosts\analysis\common-report.json"
~~~

The known retail writer terminates the block stream with the two-byte marker 00 01 followed by exactly four zero padding bytes. The inspector recognizes that exact marker and reports the four padding bytes. Arbitrary trailing data and truncated blocks are not accepted as complete.

### 2. tools/ps3_zone_script_export.py

Scans an already inflated .zone and exports validated .gsc, .csc, and compiled GSC magic assets. It recognizes the short and long IW6 headers, validates lengths and safe asset paths, writes a JSON inventory, and never edits the source zone.

~~~powershell
python tools/ps3_zone_script_export.py "C:\Ghosts\analysis\common.zone" "C:\Ghosts\analysis\scripts" --report-json "C:\Ghosts\analysis\scripts.json"
~~~

After decompiling compiled assets with GSC Tool, inventory likely player/session and Campaign hooks:

~~~powershell
python tools/gsc_campaign_audit.py "C:\Ghosts\analysis\scripts" --report-json "C:\Ghosts\analysis\gsc-audit.json"
~~~

The exporter deliberately does not classify assets by a hard-coded type ID. Public references disagree on some IW6 asset-type IDs, and names plus header shape plus GSC magic are safer for this first-pass script inventory. Embedded rawfiles inside mptype map structures are not yet extracted; that requires a separate MapEnts parser.

## Test status and limitations

Both tools have synthetic-fixture tests for patch/base marker offsets, outer block decoding, inner zlib inflation, short/long asset headers, compiled GSC detection, malformed inputs, and output path safety. These tests validate the parser logic only. Neither tool has yet been run against an actual BLUS31270/BLES/NPEB retail .ff file in this workspace.

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