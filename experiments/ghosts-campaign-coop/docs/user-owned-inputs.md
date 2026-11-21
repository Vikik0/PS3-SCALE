# User-owned Ghosts PS3 input checklist

This project does not bundle retail game files. Use files extracted from the user's own legally obtained Call of Duty: Ghosts PS3 copy. Do not upload them to GitHub, commit them, or post them in an issue/PR.

## First target profile

- Title ID: BLUS31270 (US disc)
- Title Update: 1.16, installed for the same Title ID
- Console profile: CEX 4.93
- Phase 1: two local controllers, offline Campaign

Do not mix EBOOT/FastFiles from another region or title ID.

## Send these first — no full ISO or PKG needed

1. `PARAM.SFO` from the game's `PS3_GAME` folder. This lets us confirm the actual Title ID.
2. `EBOOT.BIN` from the game after TU 1.16 is installed. Keep the original filename and tell us whether it came from the disc folder or the installed update folder. This is normally a SELF container; the current ELF inspector expects a decrypted ELF, so do not rename it to `.ELF` or try random decryptors. We will inspect the container identity first and then decide what user-controlled dump is needed.
3. `common.ff` from the same title installation. This is the first base FastFile target for the existing read-only Ghosts PS3 extractor.
4. A text list or screenshot of every `.ff` filename in the same `USRDIR/zone` tree, including any `patch_*.ff` files. Do not upload all of them yet; we will choose Campaign-relevant files from the inventory.

If convenient, include the matching `patch_common_mp.ff` too. It is useful as a known local split-screen reference, but it is not a substitute for Campaign assets.

## Keep private / do not send

- PSN username/password, console ID, IDPS, PSID, account tokens or license files.
- A full game ISO/PKG. They are not needed for the first pass.
- Do not post EBOOT or FastFiles in the public GitHub repository.

## Optional local identity checks (Windows PowerShell)

Run these in the folder containing the copied files. To identify the title metadata without uploading game files to GitHub, the repository includes a free local parser:

```powershell
Get-Item .\EBOOT.BIN, .\common.ff, .\PARAM.SFO | Select-Object Name, Length
Get-FileHash .\EBOOT.BIN -Algorithm SHA256
Get-FileHash .\common.ff -Algorithm SHA256
```

For every `.ff` file in the zone directory:

```powershell
Get-ChildItem "C:\path\to\USRDIR\zone" -Filter *.ff -Recurse -File |
  Select-Object FullName, Length
```

The first upload should be only `PARAM.SFO`, `EBOOT.BIN`, `common.ff`, and the `.ff` filename/size inventory. If a file is too large for chat upload, send its size and SHA-256 first and we will choose a transfer route that does not require a paid service.

## What happens after the files arrive

1. Verify Title ID, TU identity, file sizes and hashes.
2. Run the read-only FastFile inspector on `common.ff`; compare its header and zone output to known PS3 format checks.
3. Export and validate compiled GSC/CSC assets; decompile with the free GSC Tool IW6/PS3 backend.
4. Search Campaign assets for local-player, spawn, controller, camera, checkpoint and mission-flow references.
5. Inspect the exact executable only after its SELF/ELF state is known. No hook is approved from a guessed offset.

A passing synthetic CI test is not a retail-game validation. The Campaign second-player path remains unproven until these exact user-owned files and, later, a two-controller runtime capture are analyzed.