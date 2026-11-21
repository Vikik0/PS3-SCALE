#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "usage: build-mbedtls.sh SOURCE_DIR OUTPUT_DIR" >&2
  exit 2
fi

SOURCE_DIR="$(cd "$1" && pwd)"
OUTPUT_DIR="$2"
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
CC="$(printenv CC 2>/dev/null || true)"
AR="$(printenv AR 2>/dev/null || true)"
RANLIB="$(printenv RANLIB 2>/dev/null || true)"
CFLAGS="$(printenv CFLAGS 2>/dev/null || true)"
if [[ -z "$CC" ]]; then CC=cc; fi
if [[ -z "$AR" ]]; then AR=ar; fi
if [[ -z "$RANLIB" ]]; then RANLIB=ranlib; fi
if [[ -z "$CFLAGS" ]]; then CFLAGS="-O2"; fi

if [[ ! -f "$SOURCE_DIR/library/ssl_tls.c" || ! -f "$SOURCE_DIR/include/mbedtls/ssl.h" ]]; then
  echo "Invalid mbedTLS source directory: $SOURCE_DIR" >&2
  exit 1
fi

rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR/obj" "$OUTPUT_DIR/include"

for source in "$SOURCE_DIR"/library/*.c; do
  name="$(basename "$source" .c)"
  "$CC" $CFLAGS -std=c99 -I"$SOURCE_DIR/include" -I"$PROJECT_DIR" -DMBEDTLS_CONFIG_FILE='"cb_mbedtls_config.h"' -c "$source" -o "$OUTPUT_DIR/obj/$name.o"
done

"$AR" rcs "$OUTPUT_DIR/libcb_mbedtls.a" "$OUTPUT_DIR"/obj/*.o
if command -v "$RANLIB" >/dev/null 2>&1; then "$RANLIB" "$OUTPUT_DIR/libcb_mbedtls.a"; fi
cp -R "$SOURCE_DIR/include/mbedtls" "$OUTPUT_DIR/include/"
cp "$PROJECT_DIR/cb_mbedtls_config.h" "$OUTPUT_DIR/include/"
test -s "$OUTPUT_DIR/libcb_mbedtls.a"
test -s "$OUTPUT_DIR/include/mbedtls/ssl.h"
echo "Built mbedTLS static library: $OUTPUT_DIR/libcb_mbedtls.a"
