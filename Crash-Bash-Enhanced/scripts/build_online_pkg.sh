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
APP_ID="CBASH0001"
CONTENT_ID="UP0001-CBASH0001_00-0000000000000001"
APP_TITLE="Crash Bash Online"

test -n "${PS3DEV:-}"
test -n "${PSL1GHT:-}"
command -v ppu-gcc >/dev/null
command -v ppu-g++ >/dev/null
command -v spu-gcc >/dev/null
command -v make_self_npdrm >/dev/null
command -v pkg >/dev/null
command -v sfo >/dev/null
rm -rf "$WORK"
mkdir -p "$WORK" "$ROOT/build-artifacts"

clone_at_commit() {
    local url="$1" destination="$2" commit="$3"
    git clone --filter=blob:none --no-checkout "$url" "$destination"
    git -C "$destination" fetch --depth 1 origin "$commit"
    git -C "$destination" checkout --detach "$commit"
}

echo "== Fetch pinned PCSX-ReARMed PS3 source"
clone_at_commit "https://github.com/Calyps0man/pcsx_rearmed_for_ps3.git" "$PCSX" "$PCSX_COMMIT"
git -C "$PCSX" submodule update --init --recursive
python3 "$ROOT/scripts/patch_psxonps3.py" "$PCSX"
RSX_HEADER="$PSL1GHT/ppu/include/rsx/rsx_function_macros.h"
if ! grep -q '"memory"' "$RSX_HEADER"; then
    patch -p1 -d "$PSL1GHT" < "$PCSX/psl1ght-patches/0001-rsx-context-callback-clobbers.patch"
fi
grep -q '"memory"' "$RSX_HEADER"

echo "== Build the PS1 core for the PS3"
make -C "$PCSX" -f Makefile.libretro platform=psl1ght DYNAREC=lightrec -j"$JOBS"
CORE_ARCHIVE="$PCSX/pcsx_rearmed_libretro_psl1ght.a"
test -s "$CORE_ARCHIVE"

echo "== Fetch pinned RetroArch Netplay frontend"
clone_at_commit "https://github.com/libretro/RetroArch.git" "$RA" "$RA_COMMIT"
git -C "$RA" submodule update --init --recursive
echo "== Fetch official RetroArch menu assets"
clone_at_commit "https://github.com/libretro/retroarch-assets.git" "$ASSETS" "$ASSETS_COMMIT"

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
        (r'(?m)^CONTENTID\s*=\s*UP0001-SSNE10001_00-0000000000000001$', f"CONTENTID\\t\\t= {content_id}"),
        (r'(?m)^APPID\s*=\s*SSNE10001$', f"APPID\\t\\t\\t= {app_id}"),
        (r'(?m)^TITLE\s*=\s*Retroarch PSL1GHT$', f"TITLE\\t\\t\\t= {title}"),
    )
    for pattern, replacement in replacements:
        text, count = re.subn(pattern, replacement, text, count=1)
        if count != 1:
            raise SystemExit(f"Could not patch {filename}: {pattern}")
    path.write_text(text)
platform = root / "frontend/drivers/platform_ps3.c"
text = platform.read_text()
old = '#define EMULATOR_CONTENT_DIR "SSNE10001"'
if old not in text:
    raise SystemExit("Could not find the PSL1GHT app data directory marker")
platform.write_text(text.replace(old, f'#define EMULATOR_CONTENT_DIR "{app_id}"', 1))
config = root / "config.def.h"
text = config.read_text()
for old, new in (
    ("#define DEFAULT_NETPLAY_NAT_TRAVERSAL false", "#define DEFAULT_NETPLAY_NAT_TRAVERSAL true"),
    ("#define DEFAULT_NETPLAY_USE_MITM_SERVER false", "#define DEFAULT_NETPLAY_USE_MITM_SERVER true"),
    ('#define DEFAULT_NETPLAY_MITM_SERVER "nyc"', '#define DEFAULT_NETPLAY_MITM_SERVER "madrid"'),
):
    if old not in text:
        raise SystemExit(f"Could not find RetroArch netplay default: {old}")
    text = text.replace(old, new, 1)
if "#define DEFAULT_NETPLAY_PUBLIC_ANNOUNCE true" not in text:
    raise SystemExit("Public netplay lobby announcement is not enabled")
if "#define DEFAULT_NETPLAY_MAX_CONNECTIONS 3" not in text:
    raise SystemExit("Unexpected default player capacity")
config.write_text(text)
makefile = (root / "Makefile.psl1ght").read_text()
for required in ("-DHAVE_NETWORKING=1", "-DHAVE_SOCKET_LEGACY=1"):
    if required not in makefile:
        raise SystemExit(f"PS3 RetroArch build is missing networking flag {required}")
PY

PKGDIR="$RA/pkg/psl1ght/pkg"
USRDIR="$PKGDIR/USRDIR"
mkdir -p "$USRDIR/cores" "$USRDIR/info" "$USRDIR/assets" "$USRDIR/licenses"

echo "== Build PS3 Salamander EBOOT"
rm -f "$USRDIR/EBOOT.BIN"
make -C "$RA" -f Makefile.psl1ght.salamander clean
make -C "$RA" -f Makefile.psl1ght.salamander -j"$JOBS" create-salamander CONTENTID="$CONTENT_ID"
test -s "$USRDIR/EBOOT.BIN"

echo "== Build the RetroArch frontend launcher"
make -C "$RA" -f Makefile.psl1ght clean
rm -f "$RA/retroarch_psl1ght.elf" "$RA/retroarch_psl1ght.self"
make -C "$RA" -f Makefile.psl1ght HAVE_STATIC_DUMMY=1 -j"$JOBS"
test -s "$RA/retroarch_psl1ght.self"
cp "$RA/retroarch_psl1ght.self" "$USRDIR/retroarch_psl1ght.self"

echo "== Build RetroArch with the real PCSX-ReARMed core linked"
cp "$CORE_ARCHIVE" "$RA/libretro_psl1ght.a"
make -C "$RA" -f Makefile.psl1ght clean
rm -f "$RA/retroarch_psl1ght.elf" "$RA/retroarch_psl1ght.self" "$USRDIR/cores/CORE.SELF"
make -C "$RA" -f Makefile.psl1ght -j"$JOBS" create-core CONTENTID="$CONTENT_ID"
test -s "$USRDIR/cores/CORE.SELF"
mv "$USRDIR/cores/CORE.SELF" "$USRDIR/cores/pcsx_rearmed_libretro_psl1ght.SELF"

echo "== Install core metadata, menu assets and licenses"
wget -qO "$USRDIR/info/pcsx_rearmed_libretro.info" "https://raw.githubusercontent.com/libretro/libretro-super/master/dist/info/pcsx_rearmed_libretro.info"
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

cat > "$USRDIR/retroarch.cfg" <<'EOF'
# Crash Bash Online - PS3
menu_driver = "rgui"
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

cat > "$USRDIR/README-Crash-Bash-Online.txt" <<'EOF'
CRASH BASH ONLINE — PLAYSTATION 3

This is a PS3 homebrew package containing RetroArch Netplay and the
PCSX-ReARMed PlayStation 1 emulator core. It does not include the copyrighted
Crash Bash game disc image or Sony PlayStation BIOS files.

GETTING THE GAME
1. Make a BIN/CUE dump from your own Crash Bash disc.
2. Copy both files to /dev_hdd0/PSXISO/ on each PS3.
3. Keep the BIN filename inside the CUE file exactly matched to the BIN name.
4. If you use a PlayStation BIOS, place your legally dumped BIOS in the
   emulator's system directory. The core can also use its built-in HLE BIOS.

ONLINE PLAY
1. Both players install this same PKG and use the same Crash Bash disc dump.
2. Open the game from Load Content.
3. Open the RetroArch Quick Menu, then Netplay.
4. One player selects Start Hosting; the other selects Refresh Netplay
   Host List and joins the matching room.
5. Use the same core version, game image, BIOS choice and core options.

The build enables public room announcements, NAT traversal and the Madrid
relay endpoint by default. Network conditions and the relay service still
affect whether two consoles can connect. If a direct connection is needed,
forward TCP/UDP port 55435 on the host router.

This is an unofficial homebrew package and is not affiliated with Sony,
Activision or the original Crash Bash developers.
EOF

echo "== Package Crash Bash Online"
OUT="$ROOT/build-artifacts/Crash-Bash-Online-PS3.pkg"
pkg --contentid "$CONTENT_ID" "$PKGDIR/" "$OUT"
test -s "$OUT"
MAGIC="$(od -An -tx1 -N4 "$OUT" | tr -d ' \n')"
test "$MAGIC" = "7f504b47"
sha256sum "$OUT"
ls -lah "$OUT"
