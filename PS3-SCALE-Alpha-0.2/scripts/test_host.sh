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
  "$ROOT/source/core/ps2_config_db.c" \
  "$ROOT/tests/test_ps2_config_db.c" \
  -o "$OUT/test_ps2_config_db"
"$OUT/test_ps2_config_db"

cc -std=c99 -D_XOPEN_SOURCE=700 -DPS3SCALE_HOST_TEST=1 -Wall -Wextra -Werror \
  -I"$ROOT/include" \
  "$ROOT/source/core/ps2_config_store.c" \
  "$ROOT/tests/test_ps2_config_store.c" \
  -o "$OUT/test_ps2_config_store"
"$OUT/test_ps2_config_store"

cc -std=c99 -D_XOPEN_SOURCE=700 -Wall -Wextra -Werror \
  -I"$ROOT/include" \
  "$ROOT/source/core/atomic_file.c" \
  "$ROOT/tests/test_atomic_file.c" \
  -o "$OUT/test_atomic_file"
"$OUT/test_atomic_file"

cc -std=c99 -Wall -Wextra -Werror \
  -I"$ROOT/include" \
  "$ROOT/source/core/support_payload.c" \
  "$ROOT/tests/test_support_payload.c" \
  -o "$OUT/test_support_payload"
"$OUT/test_support_payload"

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

cc -std=c99 -Wall -Wextra -Werror \
  -I"$ROOT/include" \
  "$ROOT/source/core/game.c" \
  "$ROOT/source/core/catalog.c" \
  "$ROOT/tests/test_catalog.c" \
  -o "$OUT/test_catalog"
"$OUT/test_catalog"

cc -std=c99 -Wall -Wextra -Werror \
  -I"$ROOT/include" \
  "$ROOT/source/core/community.c" \
  "$ROOT/source/core/game.c" \
  "$ROOT/tests/test_community.c" \
  -o "$OUT/test_community"
"$OUT/test_community" "$ROOT/data/community_game_files.tsv"

python3 "$ROOT/tests/test_community_index_builder.py"

cc -std=c99 -Wall -Wextra -Werror \
  -I"$ROOT/include" \
  "$ROOT/source/core/webman_command.c" \
  "$ROOT/tests/test_webman_command.c" \
  -o "$OUT/test_webman_command"
"$OUT/test_webman_command"

cc -std=c99 -D_XOPEN_SOURCE=700 -DPS3SCALE_HOST_TEST=1 \
  -DWEBMAN_HTTP_PORT=19876 -DWEBMAN_CONNECT_TIMEOUT_SEC=1 \
  -DWEBMAN_IO_TIMEOUT_SEC=1 -Wall -Wextra -Werror -pthread \
  -I"$ROOT/include" \
  "$ROOT/source/core/webman_command.c" \
  "$ROOT/source/ps3/webman_control.c" \
  "$ROOT/tests/test_webman_http.c" \
  -o "$OUT/test_webman_http"
"$OUT/test_webman_http"

cc -std=c99 -Wall -Wextra -Werror \
  -DPS3SCALE_WEBMAN_TMP_DIR='"/tmp/ps3scale-profile-apply-test"' \
  -DPS3SCALE_WEBMAN_SCRIPT_DIR='"/tmp/ps3scale-profile-apply-test/wm_ingame"' \
  -I"$ROOT/include" \
  "$ROOT/source/ps3/profile_apply.c" \
  "$ROOT/tests/test_profile_apply.c" \
  -o "$OUT/test_profile_apply"
"$OUT/test_profile_apply"

python3 "$ROOT/tests/test_ui_contract.py" "$ROOT/source/ps3/main.c"
python3 "$ROOT/tests/test_launch_responsiveness.py" "$ROOT/source/ps3/main.c"
python3 "$ROOT/tests/test_online_download_progress.py" "$ROOT"
python3 "$ROOT/tests/test_support_service.py" "$ROOT"
python3 "$ROOT/tests/test_theme_bundle.py" "$ROOT"