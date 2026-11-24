#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GAME_DIR="${CRASH_BASH_GAME_DIR:-$ROOT/game}"
RETROARCH_DIR="$ROOT/pkgfiles/USRDIR/retroarch"
DISC_DEST="$ROOT/pkgfiles/USRDIR/PSXISO"
OUT_DIR="$ROOT/build-artifacts"
OUT="$OUT_DIR/Crash-Bash-Online-Personal-PS3.pkg"

test -n "${PS3DEV:-}" || { echo "PS3DEV is not set." >&2; exit 1; }
test -n "${PSL1GHT:-}" || { echo "PSL1GHT is not set." >&2; exit 1; }
command -v ppu-gcc >/dev/null
command -v make_self_npdrm >/dev/null
command -v pkg >/dev/null
command -v package_finalize >/dev/null
command -v sfo >/dev/null

test -s "$RETROARCH_DIR/EBOOT.BIN" || {
    echo "Embedded RetroArch runtime is missing at $RETROARCH_DIR." >&2
    exit 1
}
test -s "$RETROARCH_DIR/cores/pcsx_rearmed_libretro_psl1ght.SELF" || {
    echo "PCSX-ReARMed core metadata SELF is missing." >&2
    exit 1
}

if [[ -n "${CRASH_BASH_DISC_ARCHIVE:-}" ]]; then
    test -s "$CRASH_BASH_DISC_ARCHIVE"
    python3 "$ROOT/scripts/stage_game_disc.py" \
        --source "$GAME_DIR" \
        --destination "$DISC_DEST" \
        --archive "$CRASH_BASH_DISC_ARCHIVE" \
        --required
else
    python3 "$ROOT/scripts/stage_game_disc.py" \
        --source "$GAME_DIR" \
        --destination "$DISC_DEST" \
        --required
fi

# Replace the generic external-disc instructions with accurate instructions for
# this personal package, which embeds the supplied Crash Bash BIN/CUE pair.
PERSONAL_README="$RETROARCH_DIR/README-Crash-Bash-Online.txt"
cat > "$PERSONAL_README" <<'EOF'
CRASH BASH ONLINE — PERSONAL GAME-INCLUSIVE PS3 PACKAGE

This single PKG includes the Crash Bash game disc dump under:
  /dev_hdd0/game/CBON00001/USRDIR/PSXISO/

The built-in lobby launches the embedded RetroArch runtime and PCSX-ReARMed
core. No second PKG or external game copy is required for this included disc.

A PlayStation BIOS is not included. You may use HLE, or place a BIOS dumped
from your own console in:
  /dev_hdd0/game/CBON00001/USRDIR/retroarch/system/

ONLINE PLAY
1. Install this same single PKG on every compatible PS3.
2. Launch Crash Bash Online from the XMB.
3. Create or join a room in the built-in lobby.
4. Every player must use the same embedded game dump, BIOS mode and core options.
5. The host starts the match after all players are ready.

Netplay is independent of PSN but requires an internet connection. Four-player
performance and full hardware compatibility still require testing on the target
PS3 consoles.
EOF

make -C "$ROOT" clean
make -C "$ROOT" pkg
test -s "$ROOT/crashbash.pkg"
test -s "$ROOT/build/pkg/USRDIR/EBOOT.BIN"
test -s "$ROOT/build/pkg/USRDIR/retroarch/EBOOT.BIN"
test -s "$ROOT/build/pkg/USRDIR/retroarch/cores/pcsx_rearmed_libretro_psl1ght.SELF"
test -s "$ROOT/build/pkg/USRDIR/PSXISO/DISC-MANIFEST.txt"
test -n "$(find "$ROOT/build/pkg/USRDIR/PSXISO" -maxdepth 1 -type f -iname '*.cue' -print -quit)"
test -n "$(find "$ROOT/build/pkg/USRDIR/PSXISO" -maxdepth 1 -type f -iname '*.bin' -print -quit)"
while IFS= read -r -d '' source_file; do
    packaged_file="$ROOT/build/pkg/USRDIR/PSXISO/${source_file##*/}"
    test -s "$packaged_file"
    cmp -s "$source_file" "$packaged_file" || {
        echo "Packaged disc file differs from staged source: ${source_file##*/}" >&2
        exit 1
    }
done < <(find "$DISC_DEST" -maxdepth 1 -type f \( -iname '*.cue' -o -iname '*.bin' \) -print0)

MAGIC="$(od -An -tx1 -N4 "$ROOT/crashbash.pkg" | tr -d ' \n')"
REVISION="$(od -An -tx1 -j4 -N2 "$ROOT/crashbash.pkg" | tr -d ' \n')"
test "$MAGIC" = "7f504b47"
test "$REVISION" = "8000"

mkdir -p "$OUT_DIR"
cp "$ROOT/crashbash.pkg" "$OUT"
(
    cd "$OUT_DIR"
    sha256sum "$(basename "$OUT")" > "$(basename "$OUT").sha256"
    sha256sum -c "$(basename "$OUT").sha256"
)
{
    echo "Package: Crash Bash Online — personal game-inclusive PS3 application"
    echo "Application ID: CBON00001"
    echo "Installable packages: one PKG only"
    echo "Embedded game path: USRDIR/PSXISO/"
    cat "$DISC_DEST/DISC-MANIFEST.txt"
    echo "PlayStation BIOS: not included; use your own BIOS or HLE consistently"
    echo "Public CI upload: disabled; this personal package is kept local"
} > "$OUT_DIR/PERSONAL-BUILD-MANIFEST.txt"

echo "Personal game-inclusive PKG:"
ls -lh "$OUT" "$OUT.sha256" "$OUT_DIR/PERSONAL-BUILD-MANIFEST.txt"
