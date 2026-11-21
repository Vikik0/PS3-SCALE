#!/usr/bin/env sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
tmp="${TMPDIR:-/tmp}/cb-protocol-test-$$"
tmp_json="${TMPDIR:-/tmp}/cb-json-test-$"
tmp_archive="${TMPDIR:-/tmp}/cb-archive-test-$"
trap 'rm -f "$tmp" "$tmp_json" "$tmp_archive" "$tmp_netcode" "$ROOT/cb_archive_test.dat"' EXIT
cc -std=c11 -Wall -Wextra -Werror "$ROOT/tests/test_protocol.c" -o "$tmp"
"$tmp"
cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/include" "$ROOT/source/json_min.c" "$ROOT/tests/test_json.c" -o "$tmp_json"
"$tmp_json"
cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/include" "$ROOT/source/archive.c" "$ROOT/tests/test_archive.c" -o "$tmp_archive"
(cd "$ROOT" && "$tmp_archive")
cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/include" "$ROOT/source/netcode.c" "$ROOT/tests/test_netcode.c" -o "$tmp_netcode"
"$tmp_netcode"
echo "Host protocol, JSON, archive and netcode tests passed."
