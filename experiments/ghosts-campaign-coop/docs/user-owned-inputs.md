# User-owned Ghosts PS3 input checklist

## Current status (2026-11-22)

**No additional file transfer is requested now.** The installed TU 1.16 PARAM.SFO, the three TU SELF files, the base disc samples, and the two TU patch FastFiles have already been received and inspected. The current Campaign mission sample is Clockwork only; full-campaign coverage is not yet established.

RPCS3 is **optional**. You do not need to install it to play the final project on your PS3, and the project can continue with static FastFile/GSC analysis without it. A decrypted TU 1.16 ELF would be useful for native executable analysis, but do not install RPCS3 just for this stage and do not send console secrets or license material.


This project does not bundle retail game files. Use files extracted from the user's own legally obtained Call of Duty: Ghosts PS3 copy. Do not upload them to GitHub, commit them, or post them in an issue/PR.

## First target profile

- Title ID: BLUS31270 (US disc)
- Title Update: 1.16, installed for the same Title ID
- Console profile: CEX 4.93
- Phase 1: two local controllers, offline Campaign

Do not mix EBOOT/FastFiles from another region or title ID.

## Files already received and checked (2026-11-21)

The uploaded `PARAM(1).SFO` was parsed successfully:

| Field | Value |
|---|---|
| TITLE | Call of Duty® Ghosts |
| TITLE_ID | BLUS31270 |
| APP_VER | 01.00 |
| VERSION | 01.00 |
| PS3_SYSTEM_VER | 04.4600 |
| CATEGORY | DG |

This confirms the US BLUS31270 disc identity. It is the base disc metadata (01.00), not proof that TU 1.16 is installed.

The accompanying screenshot shows the path `PS3_GAME/USRDIR` inside the USA ISO and visible entries including `ui_mp.ff`, `ui_install.ff`, `ui.ff`, language folders and multiple `soundfile*.pak` files. The screenshot is only a partial directory view; it does not establish that EBOOT or the Campaign FastFiles are missing.

## Important path correction

The previous request for `USRDIR/zone` was too specific and should not be followed blindly. For the PS3 retail Ghosts layout, FastFiles are commonly stored directly in `PS3_GAME/USRDIR`, with localized FastFiles in language subfolders. “Zone” also refers to the decompressed internal data of a FastFile; it is not necessarily a directory on the disc.

The base zone is identified in current PS3 research as `ghosts_ps3_common.ff`; some inventories call the corresponding base file `common.ff`. Keep the original filename and do not rename it. The correct files for this dump will be selected from the complete recursive `.ff` inventory, not from an assumed folder name.

## First-pass files to identify — no full ISO upload needed

1. `EBOOT.BIN` from the original disc's `PS3_GAME/USRDIR`, if present. Keep the original filename.
2. `EBOOT.BIN` from the installed TU 1.16 update folder, separately labelled. Do not overwrite or confuse it with the disc EBOOT.
3. The base FastFile named `ghosts_ps3_common.ff` or `common.ff`, whichever exists in this exact dump.
4. A recursive text inventory of every `.ff` file under `PS3_GAME/USRDIR` and its subfolders, with file size. We will select Campaign-relevant mission files from this inventory.
5. If available from the matching TU 1.16 install, `patch_common_mp.ff` as an optional Multiplayer split-screen reference. It is not a substitute for Campaign assets.

Do not upload every FastFile yet. A filename/size list is enough to choose the next few files.

## Official matching update

A public PS3 update catalogue lists the BLUS31270 Title Update 1.16 package as:

- Filename: `UP0002-BLUS31270_00-CALLDUTYGHOSTTU1-A0116-V0100-PE.pkg`
- Size: 558,341,392 bytes
- Official Sony CDN: https://b0.ww.np.dl.playstation.net/tppkg/np/BLUS31270/BLUS31270_T16/35760cb2130fa142/UP0002-BLUS31270_00-CALLDUTYGHOSTTU1-A0116-V0100-PE.pkg

This is the update package for BLUS31270 only. It is not the full game and does not replace the user's base disc files. Do not use the NPUB31301, BLES, NPEB or Japanese packages with this disc.

## Local Windows inventory command

Run this against the extracted disc's USRDIR folder. It does not modify any game files:

```powershell
$root = "D:\Ghosts\PS3_GAME\USRDIR"
Get-ChildItem $root -Filter *.ff -Recurse -File |
  Sort-Object FullName |
  Select-Object FullName, Length |
  Export-Csv "$env:USERPROFILE\Desktop\ghosts-ff-inventory.csv" -NoTypeInformation
```

To locate the executable and base metadata:

```powershell
Get-ChildItem "D:\Ghosts\PS3_GAME" -Filter EBOOT.BIN -Recurse -File |
  Select-Object FullName, Length
Get-Item "D:\Ghosts\PS3_GAME\PARAM.SFO" | Select-Object FullName, Length
```

If the game is still open inside 7-Zip, extract the files to a normal folder first; do not try to run the inventory command against the ISO archive path.

## Keep private / do not send

- PSN username/password, console ID, IDPS, PSID, account tokens or license files.
- A full game ISO/PKG. They are not needed for the first pass.
- Do not post EBOOT or FastFiles in the public GitHub repository.

## What happens after the files arrive

1. Verify Title ID, TU identity, file sizes and hashes.
2. Run the read-only FastFile inspector on the base zone; compare its header and decompressed output to known PS3 format checks.
3. Export and validate compiled GSC/CSC assets; decompile with the free GSC Tool IW6/PS3 backend.
4. Search Campaign assets for local-player, spawn, controller, camera, checkpoint and mission-flow references.
5. Inspect the exact disc and TU EBOOTs separately. No hook is approved from a guessed offset.

A passing synthetic CI test is not a retail-game validation. The Campaign second-player path remains unproven until the exact user-owned files and, later, a two-controller runtime capture are analyzed.

## Second pass — installed TU 1.16 confirmed (2026-11-22)

The installed-update metadata file was received and parsed from the user's PS3:

| Field | Value |
|---|---|
| TITLE | Call of Duty® Ghosts |
| TITLE_ID | BLUS31270 |
| APP_VER | 01.16 |
| VERSION | 01.00 |
| CATEGORY | GD |
| PS3_SYSTEM_VER | 04.4000 |
| NP_COMMUNICATION_ID | NPWR04874_00 |

This is the installed game-data/update SFO, distinct from the previously supplied disc SFO (CATEGORY=DG, APP_VER=01.00). The update identity is now confirmed by the installed update's own metadata.

### Matching installed-update SELF files received

The following files were compared against the earlier disc-build samples. Only metadata and hashes are recorded; retail game binaries are not committed.

| File | TU file size | Disc file size | TU ELF entry | Disc ELF entry |
|---|---:|---:|---:|---:|
| EBOOT.BIN | 41,488 B | 73,704 B | 0x20080 | 0x20080 |
| default.self | 4,291,568 B | 8,072,104 B | 0x7E9508 | 0x7A8F28 |
| default_mp.self | 6,265,632 B | 9,195,256 B | 0xA75E38 | 0x8B9738 |

All three TU files are SELF v2 / key revision 28 / ELF64 big-endian PowerPC64 with 8 program headers. Their SELF type is 8, while the earlier disc samples are type 4. The executable segments 0–4 remain encrypted in the uploaded containers. This is a metadata/identity confirmation, not a decrypted code analysis.

### FastFile inputs currently on hand

- common.ff SHA-256 is 5fc31ea915894566896cc5081d3688675ecde341d42e8c38b19ad37434ca4bcb, matching the previously analysed base sample.
- clockwork.ff SHA-256 is a2ac70c7349047f2eded5f45a5e46ff8278689839fe1655106263520fe639783, matching the previously analysed Campaign mission sample.

No separate TU-specific Campaign FastFile has been verified in the current inputs. Do not treat the base FastFiles as TU 1.16 binaries.

### Current blocker

The TU 1.16 executable identity is confirmed, but the main Campaign code in default.self is still encrypted in the supplied SELF container. The next analysis input is a decrypted ELF dump of the TU 1.16 default.self; a decrypted default_mp.self dump is also useful as the reference for the game's already-supported local split-screen path. Do not send account credentials, IDPS/PSID, RAP/RIF files, or a full game image.


## Phase 7 — additional retail FastFile audit (2026-11-22)

The four locally supplied FastFiles have now been checked with the retail-aware parser: common.ff, clockwork.ff, patch_common.ff, and patch_common_mp.ff. The parser now handles the Clockwork marker at 0x16644 and the four opaque bytes after each 64 KiB raw-DEFLATE block. Exact hashes, sizes, pool counts, script/rawfile counts, and partial-tail handling are recorded in docs/phase7-retail-validation.md.

Clockwork expands to 128 MiB across 2,048 valid blocks, but 16,557,959 trailing file bytes remain uninterpreted. The MapEnts pool entry is type 0x14/index 4059 with pointer 0xFFFFFFFF. A PS3 MapEnts entity-string probe found no candidate in the validated Clockwork zone; spawn coordinates are not known. The patch_common_mp scan finds 119 compiled-script candidates against 121 ScriptFile pool entries, so two entries remain unmatched.

The current inputs still do not establish a decrypted TU 1.16 Campaign ELF or a runtime two-controller capture. No hook address or second local client is approved from the static metadata alone. The next useful native-analysis input remains a decrypted ELF of TU 1.16 default.self, plus default_mp.self for comparison; do not send console/account secrets or license files.
