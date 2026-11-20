#!/usr/bin/env sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
tmp="${TMPDIR:-/tmp}/cb-protocol-test-$$"
trap 'rm -f "$tmp"' EXIT
cc -std=c11 -Wall -Wextra -Werror "$ROOT/tests/test_protocol.c" -o "$tmp"
"$tmp"
echo "Host protocol tests passed."
