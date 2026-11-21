#!/usr/bin/env sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
tmp="${TMPDIR:-/tmp}/cb-protocol-test-$$"
tmp_json="${TMPDIR:-/tmp}/cb-json-test-$$"
trap 'rm -f "$tmp" "$tmp_json"' EXIT
cc -std=c11 -Wall -Wextra -Werror "$ROOT/tests/test_protocol.c" -o "$tmp"
"$tmp"
cc -std=c11 -Wall -Wextra -Werror -I"$ROOT/include" "$ROOT/source/json_min.c" "$ROOT/tests/test_json.c" -o "$tmp_json"
"$tmp_json"
echo "Host protocol and JSON tests passed."
