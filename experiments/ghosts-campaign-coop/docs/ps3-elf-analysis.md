# PS3 EBOOT analysis workflow (read-only)

## Purpose

The next technical blocker is identifying the real Ghosts PS3 executable layout before any Campaign hook is considered. This workflow makes the binary identity, load map, PPU OPD table and candidate offsets inspectable without guessing an image base.

## SELF-container preflight (before ELF analysis)

Run the read-only container inspector against the exact installed files:

```powershell
python tools/ps3_self_inspect.py `"EBOOT.BIN`" `"default.self`" `"default_mp.self`" --output self-container-report.json
```

The report records each file's SHA-256, SELF header version/key revision/type, app-info fields, embedded ELF class/endianness/machine/entry point, program-header count, and encrypted container segment ranges. It deliberately does not decrypt segments or infer the Title ID/TU from SELF metadata.

To confirm the installed update identity, inspect the update folder's own `PARAM.SFO` (one directory above `USRDIR`):

```powershell
python tools/ps3_sfo_inspect.py `"PARAM.SFO`"
```

The base-disc `PS3_GAME/PARAM.SFO` and the installed update's `/dev_hdd0/game/BLUS31270/PARAM.SFO` are different inputs. Do not use the former to claim that TU 1.16 is installed. `APP_VER` and `VERSION` are reported as source fields; they are not silently interpreted as a Title Update number.

A SELF report showing ELF64 big-endian PowerPC64 is only a container/architecture check. The embedded executable segments remain encrypted. Continue with the decrypted-ELF workflow below only after a valid local decrypted ELF is available.

## Input requirements

- Use a decrypted ELF64, big-endian, PowerPC64 image obtained from the user's own legally obtained game copy.
- A retail \`EBOOT.BIN\` is normally a signed/encrypted SELF container, not a plain ELF. This tool does not decrypt SELF files or supply keys.
- Keep the executable local. Do not commit it, upload it to GitHub, or put game bytes in an issue or pull request.
- Record the exact Title ID and Title Update. The current target is CEX 4.93 + TU 1.16. The CLI metadata is operator-supplied and is not inferred from the ELF.

A decrypted executable can be produced through a user-controlled PS3/RPCS3 analysis workflow. The public PS3 RE references below describe decrypted-ELF analysis; choose the method appropriate to the user's own setup. No game dump or decryption key is included here.

## Run the read-only inspector

From \`experiments/ghosts-campaign-coop\`:

\`\`\`powershell
python tools/ps3_elf_inspect.py "<local path>\\EBOOT.ELF" --title-id BLUS31270 --output eboot-report.json
\`\`\`

Replace \`BLUS31270\` with the actual installed Title ID. If the image base is independently known from a matching executable profile, it can be supplied explicitly:

\`\`\`powershell
python tools/ps3_elf_inspect.py "<local path>\\EBOOT.ELF" --title-id BLUS31270 --image-base 0x10000 --output eboot-report.json
\`\`\`

Do not pass a guessed image base. Without it, the tool tests the reported candidate offsets as direct virtual addresses only.

The report contains:

- SHA-256 and file size, so this exact executable can be identified in later notes;
- ELF class, endianness, machine and entry point;
- each \`PT_LOAD\` segment, permissions, file offset and virtual-address range;
- the section containing the ELF entry point, used as the OPD candidate range;
- PS3 PPU OPD entries (\`u32 code\`, \`u32 TOC\`), the entry descriptor and the most common module TOC (\`r2\`);
- whether each public TU 1.16 candidate address maps into a `PT_LOAD` memory segment, including zero-filled BSS (with no fabricated file bytes);
- reported `G_Client`, `G_Entity` and `scrVmPub_t` data-address leads plus the reported client/entity strides;
- a small byte window for each mapped candidate and nearby OPD navigation hints.

A mapped address is not proof that the candidate is the named function, that its calling convention is correct, or that it is safe in Campaign. OPD entries are address-taken function descriptors, not a complete list of every function.

## Optional Ghidra function seeding

1. Import the same local ELF into Ghidra as \`PowerPC:BE:64:default\`.
2. Read \`elf.opd.section_address\` and \`elf.opd.section_size\` from the JSON report.
3. Run \`tools/ghidra/SeedGhostsOpdFunctions.java\` with those two values as script arguments, for example \`0x00FF93B8 0x0004D7E0\` only if those are the values reported by this exact ELF.
4. Let Ghidra auto-analysis complete, then save the project locally. The script only seeds function starts from OPD descriptors; it does not patch or export the binary.
5. Run `tools/ghidra/FindGhostsSplitScreenBuiltins.java`. It searches loaded memory blocks for the IW6 split-screen/player builtin names and prints recorded cross-references and containing functions. A missing string or xref is inconclusive; names may be stripped, hashed or stored outside the loaded ELF.

The two Ghidra scripts are manual analysis helpers and are not compiled by GitHub CI because the Ghidra runtime is not part of the build. Verify them against the installed Ghidra version before running. The PS3 PPU function-descriptor layout and OPD-based function discovery are described in the public [ps3recomp PPU loader](https://github.com/sp00nznet/ps3recomp/blob/master/tools/ppu_loader.py) and [PS3 PPC reverse-engineering workflow](https://github.com/Mercenaries-Fan-Build/notes-on-the-released-game/blob/main/docs/ps3_ppc_re_workflow.md). Those are methodology references, not Ghosts offsets or proof of Ghosts runtime behavior.

## What to inspect after import

Use the exact EBOOT and build identity to investigate the following groups. Do not assume an offset from another platform is valid:

1. **Local-player/session setup:** the code that creates local slots, binds controllers and enters a local player into the world in an existing offline two-player mode.
2. **Client/player ownership:** the relationship between client slot, \`gclient\`, player state, player entity and command buffer.
3. **View ownership:** per-player camera/refdef, viewport, HUD client number and audio listener.
4. **Campaign entry:** compare the above state during Campaign startup with the state from the known-working local mode.
5. **Lifecycle:** join, disconnect, death, respawn, checkpoint reload, mission restart and map transition.

The first viable Campaign implementation must preserve Logan as player 1 and create a real second local player/client lifecycle. A clone/corpse entity is not enough.

## Stop conditions

Stop before any hook or patch if:

- the file is still SELF rather than decrypted ELF;
- the machine type or endianness does not match PPU;
- the actual Title ID/TU is unknown or mismatched;
- the OPD range cannot be established;
- a candidate does not map into an executable segment;
- the candidate's function boundary, call convention or runtime context is unknown.

No EBOOT, SPRX or PKG should be built for installation until a candidate is verified against the exact target executable and the hook can be tested safely.
