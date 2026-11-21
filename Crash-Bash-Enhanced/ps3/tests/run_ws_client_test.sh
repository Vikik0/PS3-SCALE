#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${TMPDIR:-/tmp}/crash-bash-ws-test-$$"
mkdir -p "$BUILD_DIR"
SERVER_PID=""

cleanup() {
  if [[ -n "$SERVER_PID" ]]; then
    kill "$SERVER_PID" 2>/dev/null || true
    wait "$SERVER_PID" 2>/dev/null || true
  fi
  rm -rf "$BUILD_DIR"
}
trap cleanup EXIT

gcc -std=c11 -D_POSIX_C_SOURCE=200112L -Wall -Wextra -Werror -I"$ROOT" \
  "$ROOT/ws_client.c" "$ROOT/tests/test_ws_client.c" \
  -o "$BUILD_DIR/test_ws_client"

node "$ROOT/tests/ws_server.mjs" >"$BUILD_DIR/server.log" 2>&1 &
SERVER_PID="$!"
for _ in $(seq 1 50); do
  if grep -q "ready" "$BUILD_DIR/server.log"; then break; fi
  sleep 0.1
done
if ! grep -q "ready" "$BUILD_DIR/server.log"; then
  cat "$BUILD_DIR/server.log"
  echo "WebSocket test server did not start." >&2
  exit 1
fi

"$BUILD_DIR/test_ws_client"
