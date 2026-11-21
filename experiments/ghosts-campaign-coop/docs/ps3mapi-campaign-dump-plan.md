# TU 1.16 Campaign runtime dump plan (no RPCS3)

Purpose: obtain the decrypted runtime bytes of the Campaign SELF from the user's own PS3 after the loader has decrypted and mapped it. This is a read-only process-memory dump; it does not patch or write to the game process.

## Exact PT_LOAD ranges from the supplied default.self

| Segment | Virtual address | p_filesz | p_memsz |
|---|---:|---:|---:|
| 0 | 0x10000 | 0x7C2428 | 0x7C2428 |
| 1 | 0x7E0000 | 0x1D1E0 | 0x1E17118 |

ELF entry point: 0x7E9508. The reconstruction utility uses the SELF's own ELF/program headers and trims the 64 KiB-rounded PS3MAPI dumps to each p_filesz.

## webMAN MOD path

webMAN MOD supports per-game batch files at /dev_hdd0/tmp/wm_ingame/<TITLE_ID>.bat and executes them when the game ID starts. It also supports manually executing a .bat via /play.ps3/<script-path>. PS3MAPI must be enabled in the XMB/In-Game PAD Shortcuts settings. The getmem.ps3mapi command supports process memory dumps by PID, start address, length, and dump size.

Project script: ghosts_tu116_campaign_dump.bat

Install path: /dev_hdd0/tmp/wm_ingame/BLUS31270.bat

The script checks TITLE_ID=BLUS31270, runs once using a completion marker, dumps the two PT_LOAD ranges from the current process, and renames the generated /dev_hdd0/mem_dump.bin files to:

- /dev_hdd0/GHOSTS_TU116_CAMPAIGN_SEG0.bin
- /dev_hdd0/GHOSTS_TU116_CAMPAIGN_SEG1.bin

After both files are copied from the PS3 once, reconstruct with:

python reconstruct_elf_from_ps3mapi_dumps.py default.self dumps/ exec_campaign_tu116.elf --report exec_campaign_tu116.json

## Constraints

- Requires webMAN MOD with PS3MAPI available and enabled.
- The current process must be the installed BLUS31270 game.
- Dumps are read-only and may include runtime relocations applied by the PS3 loader.
- The reconstruction keeps ELF64 big-endian PowerPC64 program headers and entry point but omits section headers.
- This produces an analysis ELF, not a runnable replacement EBOOT or a finished co-op mod.

Source implementation and instructions are included in the local work package.