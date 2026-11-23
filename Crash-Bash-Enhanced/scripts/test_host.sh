#!/usr/bin/env sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
tmp="${TMPDIR:-/tmp}/cb-protocol-test-$$"
tmp_json="${TMPDIR:-/tmp}/cb-json-test-$$"
tmp_archive="${TMPDIR:-/tmp}/cb-archive-test-$$"
tmp_netcode="${TMPDIR:-/tmp}/cb-netcode-test-$$"
tmp_peer="${TMPDIR:-/tmp}/cb-peer-test-$$"
tmp_disc="${TMPDIR:-/tmp}/cb-disc-test-$$"
tmp_launch="${TMPDIR:-/tmp}/cb-launch-test-$$"
tmp_fingerprint="${TMPDIR:-/tmp}/cb-fingerprint-test-$$"
trap 'rm -f "$tmp" "$tmp_json" "$tmp_archive" "$tmp_netcode" "$tmp_peer" "$tmp_disc" "$tmp_launch" "$ROOT/cb_archive_test.dat" "$tmp_fingerprint"' EXIT

cc -std=c11 -Wall -Wextra -Werror "$ROOT/tests/test_protocol.c" -o "$tmp"
"$tmp"

cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/include" "$ROOT/source/json_min.c" "$ROOT/tests/test_json.c" -o "$tmp_json"
"$tmp_json"

cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/include" "$ROOT/source/archive.c" "$ROOT/tests/test_archive.c" -o "$tmp_archive"
(cd "$ROOT" && "$tmp_archive")

cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/include" "$ROOT/source/netcode.c" "$ROOT/tests/test_netcode.c" -o "$tmp_netcode"
"$tmp_netcode"

cc -std=c11 -Wall -Wextra -Werror -D_POSIX_C_SOURCE=200809L -I"$ROOT/include" "$ROOT/source/netcode.c" "$ROOT/source/cb_peer.c" "$ROOT/tests/test_peer.c" -o "$tmp_peer"
"$tmp_peer"

cc -std=c11 -Wall -Wextra -Werror -D_POSIX_C_SOURCE=200809L -I"$ROOT/include" "$ROOT/source/disc_browser.c" "$ROOT/tests/test_disc_browser.c" -o "$tmp_disc"
"$tmp_disc"

cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/include" "$ROOT/source/game_launch_args.c" "$ROOT/tests/test_game_launch_args.c" -o "$tmp_launch"
"$tmp_launch"

cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/include" "$ROOT/source/disc_fingerprint.c" "$ROOT/tests/test_disc_fingerprint.c" -o "$tmp_fingerprint"
"$tmp_fingerprint"

python3 "$ROOT/tests/test_extract_disc.py"
python3 "$ROOT/tests/test_stage_game_disc.py"
python3 "$ROOT/tests/test_online_menu_source.py"
echo "Host protocol, JSON, archive, netcode, UDP peer, disc browser, disc fingerprint, game launch args, disc extractor, BIN/CUE staging and native lobby tests passed."
