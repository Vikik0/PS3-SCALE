#!/usr/bin/env sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
if [ -z "${PSL1GHT:-}" ]; then
  echo "ERROR: PSL1GHT is not configured."
  echo "Required: PS3DEV + PSL1GHT + Tiny3D/libfont portlibs."
  exit 2
fi
cd "$ROOT"
make clean || true
make pkg
echo "Build completed. Check for ps3scale.pkg in the project root."
