#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
FONT_DIR="$ROOT/pkgfiles/USRDIR"
FONT_COMMIT="5bb28733cec313b63fbd415b574b5ae35c8aa8b8"
FONT_BLOB_SHA="7852c6ee5f44d304c03fa2e95c52563ad4c526a1"
OFL_BLOB_SHA="94d1bf7b52481343e2983fb7751e8b7abe1ea07b"

mkdir -p "$FONT_DIR"

FONT_URL="https://raw.githubusercontent.com/google/fonts/$FONT_COMMIT/ofl/notosansarabicui/NotoSansArabicUI%5Bwdth%2Cwght%5D.ttf"
OFL_URL="https://raw.githubusercontent.com/google/fonts/$FONT_COMMIT/ofl/notosansarabicui/OFL.txt"

curl --fail --location --silent --show-error --retry 3 \
    "$FONT_URL" -o "$FONT_DIR/NotoSansArabicUI.ttf"
curl --fail --location --silent --show-error --retry 3 \
    "$OFL_URL" -o "$FONT_DIR/OFL-NotoSansArabicUI.txt"

ACTUAL_FONT_SHA=$(git hash-object "$FONT_DIR/NotoSansArabicUI.ttf")
ACTUAL_OFL_SHA=$(git hash-object "$FONT_DIR/OFL-NotoSansArabicUI.txt")

if [ "$ACTUAL_FONT_SHA" != "$FONT_BLOB_SHA" ]; then
    echo "Noto Sans Arabic UI font hash mismatch." >&2
    exit 1
fi
if [ "$ACTUAL_OFL_SHA" != "$OFL_BLOB_SHA" ]; then
    echo "Noto Sans Arabic UI license hash mismatch." >&2
    exit 1
fi

echo "Verified Noto Sans Arabic UI font and OFL license."
