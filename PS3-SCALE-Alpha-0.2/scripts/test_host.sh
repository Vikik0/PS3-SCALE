#!/usr/bin/env sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
OUT="$ROOT/build-host"
mkdir -p "$OUT"

cc -std=c99 -Wall -Wextra -Werror \
  -I"$ROOT/include" \
  "$ROOT/source/core/sfo.c" \
  "$ROOT/tests/test_sfo.c" \
  -o "$OUT/test_sfo"
"$OUT/test_sfo"

cc -std=c99 -D_XOPEN_SOURCE=700 -Wall -Wextra -Werror \
  -I"$ROOT/include" \
  "$ROOT/source/core/sfo.c" \
  "$ROOT/source/core/game.c" \
  "$ROOT/source/core/iso9660.c" \
  "$ROOT/source/ps3/scanner.c" \
  "$ROOT/tests/test_scanner.c" \
  -o "$OUT/test_scanner"
"$OUT/test_scanner"

cc -std=c99 -Wall -Wextra -Werror \
  -I"$ROOT/include" \
  "$ROOT/source/core/iso9660.c" \
  "$ROOT/tests/test_iso9660.c" \
  -o "$OUT/test_iso9660"
"$OUT/test_iso9660"

cc -std=c99 -Wall -Wextra -Werror \
  -I"$ROOT/include" \
  "$ROOT/source/core/ps2_config.c" \
  "$ROOT/tests/test_ps2_config.c" \
  -o "$OUT/test_ps2_config"
"$OUT/test_ps2_config"

cc -std=c99 -Wall -Wextra -Werror \
  -I"$ROOT/include" \
  "$ROOT/source/core/locale.c" \
  "$ROOT/tests/test_locale.c" \
  -o "$OUT/test_locale"
"$OUT/test_locale"

cc -std=c99 -Wall -Wextra -Werror \
  -I"$ROOT/include" \
  "$ROOT/source/core/hardware_info.c" \
  "$ROOT/tests/test_hardware_info.c" \
  -o "$OUT/test_hardware_info"
"$OUT/test_hardware_info"

python3 "$ROOT/tests/test_ui_contract.py" "$ROOT/source/ps3/main.c"
python3 "$ROOT/tests/test_theme_bundle.py" "$ROOT"
