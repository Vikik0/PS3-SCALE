#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$RUNNER_TEMP/crash-bash-ps1-engine"
UPSTREAM="https://github.com/Calyps0man/pcsx_rearmed_for_ps3.git"
COMMIT="cce9fc67f46a495d4746aa49cff8e3367a9a315e"
APP_ID="CBASH0001"
APP_TITLE="Crash Bash"

rm -rf "$WORK"
git clone --filter=blob:none "$UPSTREAM" "$WORK"
git -C "$WORK" checkout --detach "$COMMIT"

python3 - "$WORK" "$APP_ID" "$APP_TITLE" <<'PY'
from pathlib import Path
import sys

root = Path(sys.argv[1])
app_id, title = sys.argv[2], sys.argv[3]
common = root / "ps3/src/common.h"
build = root / "ps3/build.sh"

text = common.read_text()
replacements = {
    '#define APP_ID    "PCSR00001"': f'#define APP_ID    "{app_id}"',
    '#define APP_TITLE "PCSX-ReARMed for PS3"': f'#define APP_TITLE "{title}"',
    '#define APP_VERSION "0.1 (build 72)"': '#define APP_VERSION "0.1 (build 1)"',
}
for old, new in replacements.items():
    if old not in text:
        raise SystemExit(f"Expected source marker not found in common.h: {old}")
    text = text.replace(old, new, 1)
common.write_text(text)

text = build.read_text()
text = text.replace(
    'CID=UP0001-PCSR00001_00-0000000000000001',
    f'CID=UP0001-{app_id}_00-0000000000000001'
)
text = text.replace(
    'sfo --title "PCSX-ReARMed for PS3" --appid PCSR00001',
    f'sfo --title "{title}" --appid {app_id}'
)
marker = 'cp "$ROOT/data/ps1titles.txt" "$B/pkg/USRDIR/"'
if marker not in text:
    raise SystemExit("Could not find package data-copy marker in ps3/build.sh")
text = text.replace(
    marker,
    marker + '\ncp "$ROOT/../COPYING" "$B/pkg/USRDIR/COPYING-GPL2.txt"\n'
    'cp "$ROOT/../CrashBashOnline-README.txt" "$B/pkg/USRDIR/README.txt"'
)
build.write_text(text)
PY

RSX_HEADER="$PSL1GHT/ppu/include/rsx/rsx_function_macros.h"
if ! grep -q '"memory"' "$RSX_HEADER"; then
  patch -p1 -d "$PSL1GHT" < "$WORK/psl1ght-patches/0001-rsx-context-callback-clobbers.patch"
fi

cat > "$WORK/CrashBashOnline-README.txt" <<'EOF'
CRASH BASH — PS3

This package contains a PlayStation 1 emulator front end. It does not contain
the copyrighted Crash Bash disc image.

To play your own Crash Bash copy:
1. Copy the matching BIN and CUE files to /dev_hdd0/PSXISO/ or a USB drive's
   /PSXISO/ folder.
2. Keep the BIN filename in the CUE file exactly matched to the actual BIN.
3. Install this PKG on a PS3 running HEN or custom firmware.
4. Open Crash Bash and select the Crash Bash (USA) disc image.

This is an emulator package, not an official Sony or Activision product.
Online multiplayer is not enabled in this base build.
EOF

cd "$WORK/ps3"
./build.sh CrashBash
mkdir -p "$ROOT/build-artifacts"
cp "$WORK/ps3/build/CrashBash.pkg" "$ROOT/build-artifacts/CrashBash-PS3-Base.pkg"
sha256sum "$ROOT/build-artifacts/CrashBash-PS3-Base.pkg"
