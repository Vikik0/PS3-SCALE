# TU 1.16 runtime capture plan — PS3MAPI

Purpose: obtain read-only runtime bytes from the user's own PS3 after webMAN/PS3MAPI has access to the loaded Ghosts process. These captures are for static analysis of the exact Campaign and Multiplayer executables; they are not patched or re-signed game files.

## Exact PT_LOAD ranges from the supplied TU 1.16 SELF files

### Campaign — default.self

| Segment | Virtual address | p_filesz | p_memsz |
|---|---:|---:|---:|
| 0 | 0x10000 | 0x7C2428 | 0x7C2428 |
| 1 | 0x7E0000 | 0x1D1E0 | 0x1E17118 |

Entry point: 0x7E9508.

### Multiplayer — default_mp.self

| Segment | Virtual address | p_filesz | p_memsz |
|---|---:|---:|---:|
| 0 | 0x10000 | 0xA48C28 | 0xA48C28 |
| 1 | 0xA60000 | 0xE39D0 | 0x1DFA898 |

Entry point: 0xA75E38.

The checked-in profile is tools/ps3mapi/SELF_MEMORY_MAP_TU116.json. The exact SELF used for verification must come from the installed TU 1.16 update, not the base disc 1.00 directory.

## webMAN MOD / PS3MAPI behavior

webMAN MOD documents the process dump command as:

/getmem.ps3mapi?proc=<id>&addr=<start-address>&len=<length>&dump=<size>

When proc is omitted, the current game process is selected while webMAN is in-game. dump is parsed as a byte count (small values may be interpreted as MB) and the dump writer reads memory in 64 KiB chunks. The len parameter controls the small memory-view window; it is not the total dump size.

References:
- https://github.com/aldostools/webMAN-MOD/wiki/Web-Commands
- https://github.com/aldostools/webMAN-MOD/blob/master/include/ps3mapi/ps3mapi.h
- https://github.com/aldostools/webMAN-MOD/blob/master/include/ps3mapi/debug_mem.h

## Manual capture scripts

The project now includes two separate read-only scripts:

- tools/ps3mapi/dump_campaign_tu116.bat
- tools/ps3mapi/dump_multiplayer_tu116.bat

**Do not install either as /dev_hdd0/tmp/wm_ingame/BLUS31270.bat.** Campaign and Multiplayer share Title ID BLUS31270, so an automatic per-game script cannot know which executable is active. Run the matching script manually only after entering the correct mode.

Run Campaign:
http://PS3_IP/play.ps3/dev_hdd0/tmp/dump_campaign_tu116.bat

It writes:
- /dev_hdd0/GHOSTS_TU116_CAMPAIGN_SEG0.bin
- /dev_hdd0/GHOSTS_TU116_CAMPAIGN_SEG1.bin

Run Multiplayer:
http://PS3_IP/play.ps3/dev_hdd0/tmp/dump_multiplayer_tu116.bat

It writes:
- /dev_hdd0/GHOSTS_TU116_MULTIPLAYER_SEG0.bin
- /dev_hdd0/GHOSTS_TU116_MULTIPLAYER_SEG1.bin

Both scripts delete only their own previous output files and webMAN's temporary mem_dump.bin before each read. They never call setmem, /patch.ps3, or any game-memory write command. A completion marker prevents accidental repeat runs; remove that marker manually only when intentionally capturing again.

## Verify and reconstruct

Copy the two Campaign segment files into one local folder and run:

~~~powershell
python tools/ps3mapi/verify_tu116_dump.py --mode campaign --self "D:\Ghosts\TU116\default.self" --dumps "D:\Ghosts\captures\campaign" --elf "D:\Ghosts\analysis\campaign_tu116.elf" --report "D:\Ghosts\analysis\campaign_tu116_report.json"
~~~

Then run the Multiplayer equivalent:

~~~powershell
python tools/ps3mapi/verify_tu116_dump.py --mode multiplayer --self "D:\Ghosts\TU116\default_mp.self" --dumps "D:\Ghosts\captures\multiplayer" --elf "D:\Ghosts\analysis\multiplayer_tu116.elf" --report "D:\Ghosts\analysis\multiplayer_tu116_report.json"
~~~

No manual renaming to SEG0.bin / SEG1.bin is required. The verifier:
- checks the entry point and every non-empty PT_LOAD tuple against the selected TU 1.16 map;
- checks exact 64 KiB-padded dump sizes;
- rejects empty/zero-filled executable segments;
- records SHA-256 for each input and output;
- refuses to overwrite the input SELF or an existing output unless --overwrite is explicit.

The output is a minimal ELF64 big-endian PowerPC64 **analysis image**. It preserves PT_LOAD headers and the entry point, omits section headers, and represents BSS with p_memsz. Runtime bytes may include loader relocations. This ELF is not a runnable replacement EBOOT.

## After reconstruction

Run the existing read-only inspector on each ELF:

~~~powershell
python tools/ps3_elf_inspect.py "D:\Ghosts\analysis\campaign_tu116.elf" --title-id BLUS31270 --output "D:\Ghosts\analysis\campaign_elf_report.json"
python tools/ps3_elf_inspect.py "D:\Ghosts\analysis\multiplayer_tu116.elf" --title-id BLUS31270 --output "D:\Ghosts\analysis\multiplayer_elf_report.json"
~~~

Then compare strings:

~~~powershell
python tools/ps3mapi/scan_runtime_strings.py "D:\Ghosts\analysis\campaign_tu116.elf" --json "D:\Ghosts\analysis\campaign_strings.json"
python tools/ps3mapi/scan_runtime_strings.py "D:\Ghosts\analysis\multiplayer_tu116.elf" --json "D:\Ghosts\analysis\multiplayer_strings.json"
python tools/ps3mapi/compare_runtime_strings.py "D:\Ghosts\analysis\campaign_tu116.elf" "D:\Ghosts\analysis\multiplayer_tu116.elf" --output "D:\Ghosts\analysis\campaign_mp_string_diff.json"
~~~

~~~powershell
python tools/ps3mapi/diff_runtime_elfs.py "D:\Ghosts\analysis\campaign_tu116.elf" "D:\Ghosts\analysis\multiplayer_tu116.elf" --output "D:\Ghosts\analysis\campaign_mp_binary_diff.json"
~~~

String presence is a navigation lead only. A matching function name or address does not prove that a function is callable from Campaign or safe to hook.

## Privacy and safety

- Keep SELF files and raw memory dumps on the user's computer. Do not commit or upload them to GitHub.
- Do not send PSN credentials, IDPS, PSID, console keys, or license files.
- Share only the JSON verification reports, SHA-256 values, and the specific findings needed for analysis unless raw bytes are explicitly required.
- Do not use a Multiplayer address as a Campaign hook without verifying its function boundary, TOC, calling convention, and runtime context.
- No EBOOT patch, FastFile rebuild, or installable PKG is produced by this capture workflow.
