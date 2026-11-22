#!/usr/bin/env python3
"""Apply the Crash Bash four-player foundation to a clean PSXonPS3 checkout."""
from pathlib import Path
import sys

root = Path(sys.argv[1]).resolve()

def replace_once(relpath: str, old: str, new: str) -> None:
    path = root / relpath
    if not path.is_file():
        raise SystemExit(f"missing upstream file: {relpath}")
    source = path.read_text(encoding="utf-8")
    count = source.count(old)
    if count != 1:
        raise SystemExit(f"{relpath}: expected exactly one match, found {count}: {old!r}")
    path.write_text(source.replace(old, new, 1), encoding="utf-8")

# Four local DualShock controllers. ioPadInit(7) already initializes the PS3
# pad service with enough slots; PORTS controls how many this front end reads.
replace_once(
    "ps3/src/input.c",
    "#define PORTS 2",
    "#define PORTS 4",
)

# The core already implements the PS1 Multitap protocol. Enable it on physical
# controller port 1, which maps to virtual player indices 0..3.
replace_once(
    "ps3/src/main.c",
    '    if (!strcmp(key, "pcsx_rearmed_drc")) return cfg.dynarec ? "enabled" : "disabled";',
    '    if (!strcmp(key, "pcsx_rearmed_drc")) return cfg.dynarec ? "enabled" : "disabled";\n'
    '    if (!strcmp(key, "pcsx_rearmed_multitap")) return "port 1";',
)
replace_once(
    "ps3/src/main.c",
    "    if (port > 1 || input_blocked) return 0;",
    "    if (port >= 4 || input_blocked) return 0;",
)
replace_once(
    "ps3/src/main.c",
    "        if (input_blocked && !p0->buttons && !input_pad(1)->buttons) input_blocked = 0;",
    "        if (input_blocked && !input_pad(0)->buttons && !input_pad(1)->buttons &&\n"
    "            !input_pad(2)->buttons && !input_pad(3)->buttons) input_blocked = 0;",
)

# Give the custom package its own title ID and data directory. The game image
# remains external and is never embedded in the PKG.
replace_once(
    "ps3/src/common.h",
    '#define APP_ID    "PCSR00001"',
    '#define APP_ID    "CRSHBASH1"',
)
replace_once(
    "ps3/src/common.h",
    '#define APP_TITLE "PCSX-ReARMed for PS3"',
    '#define APP_TITLE "Crash Bash Online"',
)
replace_once(
    "ps3/src/common.h",
    '#define APP_VERSION "0.1 (build 72)"',
    '#define APP_VERSION "0.1 (4-player foundation)"',
)
replace_once(
    "ps3/build.sh",
    "CID=UP0001-PCSR00001_00-0000000000000001",
    "CID=UP0001-CRSHBASH1_00-0000000000000001",
)
replace_once(
    "ps3/build.sh",
    'sfo --title "PCSX-ReARMed for PS3" --appid PCSR00001 -f',
    'sfo --title "Crash Bash Online" --appid CRSHBASH1 -f',
)

# Fail early if the upstream core option changed or disappeared.
options = (root / "frontend/libretro_core_options.h").read_text(encoding="utf-8")
if '"pcsx_rearmed_multitap"' not in options or '{ "port 1",        "Port 1" }' not in options:
    raise SystemExit("upstream Multitap option was not found; refusing an unverified build")

print("Crash Bash four-player foundation patch applied successfully.")
