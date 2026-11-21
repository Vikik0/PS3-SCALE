#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="${RUNNER_TEMP:-/tmp}/crash-bash-online"
PCSX="$WORK/pcsx_rearmed"
RA="$WORK/RetroArch"
ASSETS="$WORK/retroarch-assets"
JOBS="${JOBS:-$(nproc)}"

PCSX_COMMIT="cce9fc67f46a495d4746aa49cff8e3367a9a315e"
RA_COMMIT="8ede0d8aea9308e72d4f2b58c3074934803a6452"
ASSETS_COMMIT="8c918e2872482139273374ef4cc6c7aff34d7585"
LIBRETRO_SUPER_COMMIT="55e50a81195eaf8dd1b7720a601282459832ced1"

APP_ID="CBASH0001"
CONTENT_ID="UP0001-CBASH0001_00-0000000000000001"
APP_TITLE="Crash Bash Online"

test -n "${PS3DEV:-}"
test -n "${PSL1GHT:-}"
command -v ppu-gcc >/dev/null
command -v ppu-g++ >/dev/null
command -v spu-gcc >/dev/null
command -v make_self_npdrm >/dev/null
command -v make_self_wc >/dev/null
command -v pkg >/dev/null
command -v sfo >/dev/null
command -v git >/dev/null
command -v wget >/dev/null
command -v patch >/dev/null
test -s "$PS3DEV/bin/sfo.xml"

rm -rf "$WORK"
mkdir -p "$WORK" "$ROOT/build-artifacts"

clone_at_commit() {
    local url="$1"
    local destination="$2"
    local commit="$3"

    git clone --depth 1 --filter=blob:none --no-checkout "$url" "$destination"
    git -C "$destination" fetch --depth 1 origin "$commit"
    git -C "$destination" checkout --detach "$commit"
}

echo "== Fetch pinned PCSX-ReARMed PS3 source"
clone_at_commit "https://github.com/Calyps0man/pcsx_rearmed_for_ps3.git" "$PCSX" "$PCSX_COMMIT"
git -C "$PCSX" submodule update --init --recursive
python3 "$ROOT/scripts/patch_psxonps3.py" "$PCSX"
python3 "$ROOT/scripts/patch_psxonps3_retroarch.py" "$PCSX"

# The upstream PS3 RSX inline assembly needs a memory clobber for GCC.
RSX_HEADER="$PSL1GHT/ppu/include/rsx/rsx_function_macros.h"
if ! grep -q '"memory"' "$RSX_HEADER"; then
    patch -p1 -d "$PSL1GHT" < "$PCSX/psl1ght-patches/0001-rsx-context-callback-clobbers.patch"
fi
grep -q '"memory"' "$RSX_HEADER"

# Fail early if the pinned core no longer exposes the multitap option used below.
grep -q '"pcsx_rearmed_multitap"' "$PCSX/frontend/libretro_core_options.h"
grep -q '"port 1"' "$PCSX/frontend/libretro_core_options.h"

echo "== Build the PS1 core for the PS3"
make -C "$PCSX" -f Makefile.libretro platform=psl1ght DYNAREC=lightrec -j"$JOBS"
CORE_ARCHIVE="$PCSX/pcsx_rearmed_libretro_psl1ght.a"
test -s "$CORE_ARCHIVE"

echo "== Fetch pinned RetroArch Netplay frontend"
clone_at_commit "https://github.com/libretro/RetroArch.git" "$RA" "$RA_COMMIT"
git -C "$RA" submodule update --init --recursive

echo "== Fetch official RetroArch menu assets"
git clone --depth 1 --filter=blob:none --no-checkout "https://github.com/libretro/retroarch-assets.git" "$ASSETS"
git -C "$ASSETS" fetch --depth 1 origin "$ASSETS_COMMIT"
git -C "$ASSETS" sparse-checkout init --cone
git -C "$ASSETS" sparse-checkout set fonts rgui xmb ozone wallpapers
git -C "$ASSETS" checkout --detach "$ASSETS_COMMIT"

python3 - "$RA" "$APP_ID" "$CONTENT_ID" "$APP_TITLE" <<'PY'
from pathlib import Path
import re
import sys

root = Path(sys.argv[1])
app_id, content_id, title = sys.argv[2:5]

for filename in ("Makefile.psl1ght", "Makefile.psl1ght.salamander"):
    path = root / filename
    text = path.read_text()
    replacements = (
        (r"(?m)^CONTENTID\s*=\s*UP0001-SSNE10001_00-0000000000000001$", f"CONTENTID = {content_id}"),
        (r"(?m)^APPID\s*=\s*SSNE10001$", f"APPID = {app_id}"),
        (r"(?m)^TITLE\s*=\s*Retroarch PSL1GHT$", f"TITLE = {title}"),
    )
    for pattern, replacement in replacements:
        text, count = re.subn(pattern, replacement, text, count=1)
        if count != 1:
            raise SystemExit(f"Could not patch {filename}: {pattern}")
    path.write_text(text)

platform = root / "frontend/drivers/platform_ps3.c"
text = platform.read_text()
old = '#define EMULATOR_CONTENT_DIR "SSNE10001"'
if text.count(old) != 1:
    raise SystemExit("Could not uniquely identify the PSL1GHT app data directory")
platform.write_text(text.replace(old, f'#define EMULATOR_CONTENT_DIR "{app_id}"', 1))

config = root / "config.def.h"
text = config.read_text()
for old, new in (
    ("#define DEFAULT_NETPLAY_NAT_TRAVERSAL false", "#define DEFAULT_NETPLAY_NAT_TRAVERSAL true"),
    ("#define DEFAULT_NETPLAY_USE_MITM_SERVER false", "#define DEFAULT_NETPLAY_USE_MITM_SERVER true"),
    ('#define DEFAULT_NETPLAY_MITM_SERVER "nyc"', '#define DEFAULT_NETPLAY_MITM_SERVER "madrid"'),
):
    if text.count(old) != 1:
        raise SystemExit(f"Could not uniquely patch RetroArch netplay default: {old}")
    text = text.replace(old, new, 1)

required_config = (
    "#define DEFAULT_NETPLAY_PUBLIC_ANNOUNCE true",
    "#define DEFAULT_NETPLAY_MAX_CONNECTIONS 3",
)
for required in required_config:
    if required not in text:
        raise SystemExit(f"RetroArch default is not as expected: {required}")
config.write_text(text)

makefile = (root / "Makefile.psl1ght").read_text()
for required in ("-DHAVE_NETWORKING=1", "-DHAVE_SOCKET_LEGACY=1"):
    if required not in makefile:
        raise SystemExit(f"PS3 RetroArch build is missing networking flag {required}")
PY

PKGDIR="$RA/pkg/psl1ght/pkg"
USRDIR="$PKGDIR/USRDIR"
CORE_SELF="$USRDIR/cores/pcsx_rearmed_libretro_psl1ght.SELF"
mkdir -p "$USRDIR/cores" "$USRDIR/info" "$USRDIR/assets" \
    "$USRDIR/licenses" "$USRDIR/system"

echo "== Build PS3 Salamander EBOOT"
rm -f "$USRDIR/EBOOT.BIN"
make -C "$RA" -f Makefile.psl1ght.salamander clean
make -C "$RA" -f Makefile.psl1ght.salamander -j"$JOBS" create-salamander CONTENTID="$CONTENT_ID"
test -s "$USRDIR/EBOOT.BIN"

echo "== Link RetroArch directly with the real PCSX-ReARMed core"
cp "$CORE_ARCHIVE" "$RA/libretro_psl1ght.a"
make -C "$RA" -f Makefile.psl1ght clean
rm -f "$RA/retroarch_psl1ght.elf" "$RA/retroarch_psl1ght.self" \
    "$USRDIR/cores/CORE.SELF" "$CORE_SELF"
make -C "$RA" -f Makefile.psl1ght -j"$JOBS" create-core CONTENTID="$CONTENT_ID"
test -s "$USRDIR/cores/CORE.SELF"
mv "$USRDIR/cores/CORE.SELF" "$CORE_SELF"
test -s "$CORE_SELF"

echo "== Install pinned core metadata, menu assets and licenses"
wget -qO "$USRDIR/info/pcsx_rearmed_libretro.info" \
    "https://raw.githubusercontent.com/libretro/libretro-super/$LIBRETRO_SUPER_COMMIT/dist/info/pcsx_rearmed_libretro.info"
grep -q 'savestate_features = "deterministic"' "$USRDIR/info/pcsx_rearmed_libretro.info"

cp -a "$ASSETS/fonts" "$USRDIR/assets/"
cp -a "$ASSETS/rgui" "$USRDIR/assets/"
cp -a "$ASSETS/xmb" "$USRDIR/assets/"
cp -a "$ASSETS/ozone" "$USRDIR/assets/"
cp -a "$ASSETS/wallpapers" "$USRDIR/assets/"
cp "$RA/COPYING" "$USRDIR/licenses/RetroArch-GPL-3.0.txt"
cp "$PCSX/COPYING" "$USRDIR/licenses/PCSX-ReARMed-GPL-2.0.txt"
cp "$ASSETS/COPYING" "$USRDIR/licenses/RetroArch-assets-CC-BY-4.0.txt"
cp "$ROOT/pkgfiles/ICON0.PNG" "$PKGDIR/ICON0.PNG"

sfo --title "$APP_TITLE" --appid "$APP_ID" -f "$PS3DEV/bin/sfo.xml" "$PKGDIR/PARAM.SFO"

# Validate the app identity and both launchable PS3 SELF files before packaging.
python3 - "$PKGDIR/PARAM.SFO" "$APP_ID" "$APP_TITLE" "$USRDIR/EBOOT.BIN" "$CORE_SELF" <<'PY'
from pathlib import Path
import sys

sfo_path, app_id, title, eboot_path, core_path = sys.argv[1:]
sfo = Path(sfo_path).read_bytes()
if app_id.encode("ascii") not in sfo:
    raise SystemExit("PARAM.SFO does not contain the expected application ID")
if title.encode("utf-8") not in sfo:
    raise SystemExit("PARAM.SFO does not contain the expected application title")

for filename in (eboot_path, core_path):
    path = Path(filename)
    data = path.read_bytes()
    if len(data) < 4 or data[:4] != b"SCE\x00":
        raise SystemExit(f"{path.name} is not a valid PS3 SELF header")
PY

cat > "$USRDIR/retroarch.cfg" <<EOF
# Crash Bash Online - PS3
menu_driver = "rgui"
system_directory = "/dev_hdd0/game/$APP_ID/USRDIR/system"
core_options_path = "/dev_hdd0/game/$APP_ID/USRDIR/retroarch-core-options.cfg"
netplay_nickname = "CrashBashPlayer"
netplay_public_announce = "true"
netplay_use_mitm_server = "true"
netplay_mitm_server = "madrid"
netplay_nat_traversal = "true"
netplay_tcp_udp_port = "55435"
netplay_max_connections = "3"
netplay_start_as_spectator = "false"
menu_show_load_content = "true"
config_save_on_exit = "false"
EOF

cat > "$USRDIR/retroarch-core-options.cfg" <<'EOF'
pcsx_rearmed_multitap = "port 1"
EOF

cat > "$USRDIR/README-Crash-Bash-Online.txt" <<'EOF'
CRASH BASH ONLINE — PLAYSTATION 3

This package contains RetroArch Netplay and a statically linked PCSX-ReARMed
PlayStation 1 core. It does not contain the Crash Bash game disc image or a
Sony PlayStation BIOS.

GETTING THE GAME
1. Create a BIN/CUE dump from your own Crash Bash disc.
2. Copy both files to /dev_hdd0/PSXISO/ or to a /PSXISO/ folder on USB.
3. Keep the BIN filename referenced by the CUE file exactly correct.
4. A BIOS dumped from your own PS1 is recommended. If you use one, place it
   in /dev_hdd0/game/CBASH0001/USRDIR/system/ on each console.

LOCAL PLAY
1. Launch Crash Bash Online from the PS3 XMB.
2. Choose Load Content and open the game's CUE file.
3. The included core has Multitap Mode set to Port 1.

ONLINE PLAY
1. Both consoles install the same PKG and use identical game dumps.
2. Both players load the same CUE file with the included PCSX-ReARMed core.
3. Open RetroArch's Netplay menu.
4. The host selects Start Hosting.
5. Other players refresh the Netplay Host List and join the host.
6. The host accepts up to three remote connections (four players total).

Keep the game dump, region, BIOS choice, core version and core options identical
on every console. The package enables the RetroArch public lobby, NAT traversal
and the Madrid relay endpoint by default. If direct connection is needed,
forward TCP/UDP port 55435 on the host router. Network restrictions or relay
availability can still prevent a connection.

PSN is not initialized or required by this homebrew build. An internet
connection is still required for online Netplay.

This is an unofficial homebrew project and is not affiliated with Sony,
Activision or the original Crash Bash developers.
EOF

echo "== Package Crash Bash Online"
OUT="$ROOT/build-artifacts/Crash-Bash-Online-PS3.pkg"
pkg --contentid "$CONTENT_ID" "$PKGDIR/" "$OUT"
test -s "$OUT"

MAGIC="$(od -An -tx1 -N4 "$OUT" | tr -d ' \n')"
test "$MAGIC" = "7f504b47"

sha256sum "$OUT" > "$OUT.sha256"
cat > "$ROOT/build-artifacts/BUILD-MANIFEST.txt" <<EOF
Package: Crash Bash Online — PS3
Application ID: $APP_ID
Content ID: $CONTENT_ID
RetroArch commit: $RA_COMMIT
PCSX-ReARMed commit: $PCSX_COMMIT
RetroArch assets commit: $ASSETS_COMMIT
libretro-super metadata commit: $LIBRETRO_SUPER_COMMIT
Netplay: RetroArch Netplay, PSN-independent
Player limit: 1 host + 3 remote clients (4 total)
PCSX-ReARMed Multitap: Port 1
Game data and BIOS: not included
EOF

echo "Build outputs:"
ls -lah "$OUT" "$OUT.sha256" "$ROOT/build-artifacts/BUILD-MANIFEST.txt"
cat "$OUT.sha256"
