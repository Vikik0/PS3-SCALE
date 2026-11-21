#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${TMPDIR:-/tmp}/crash-bash-ws-test-$$"
WS_SERVER_PID=""
RELAY_SERVER_PID=""

cleanup() {
  if [[ -n "$WS_SERVER_PID" ]]; then
    kill "$WS_SERVER_PID" 2>/dev/null || true
    wait "$WS_SERVER_PID" 2>/dev/null || true
  fi
  if [[ -n "$RELAY_SERVER_PID" ]]; then
    kill "$RELAY_SERVER_PID" 2>/dev/null || true
    wait "$RELAY_SERVER_PID" 2>/dev/null || true
  fi
  rm -rf "$BUILD_DIR"
}
trap cleanup EXIT
mkdir -p "$BUILD_DIR"

gcc -std=c11 -D_POSIX_C_SOURCE=200112L -Wall -Wextra -Werror -I"$ROOT" \
  "$ROOT/ws_client.c" "$ROOT/tests/test_ws_client.c" \
  -o "$BUILD_DIR/test_ws_client"

gcc -std=c11 -D_POSIX_C_SOURCE=200112L -Wall -Wextra -Werror -I"$ROOT" \
  "$ROOT/ws_client.c" "$ROOT/relay_client.c" "$ROOT/tests/test_relay_client.c" \
  -o "$BUILD_DIR/test_relay_client"

node "$ROOT/tests/ws_server.mjs" >"$BUILD_DIR/ws-server.log" 2>&1 &
WS_SERVER_PID="$!"
node "$ROOT/tests/relay_server.mjs" >"$BUILD_DIR/relay-server.log" 2>&1 &
RELAY_SERVER_PID="$!"

for _ in $(seq 1 50); do
  if grep -q "ready" "$BUILD_DIR/ws-server.log" && grep -q "ready" "$BUILD_DIR/relay-server.log"; then break; fi
  sleep 0.1
done
if ! grep -q "ready" "$BUILD_DIR/ws-server.log" || ! grep -q "ready" "$BUILD_DIR/relay-server.log"; then
  cat "$BUILD_DIR/ws-server.log" "$BUILD_DIR/relay-server.log"
  echo "WebSocket test servers did not start." >&2
  exit 1
fi

"$BUILD_DIR/test_ws_client"
"$BUILD_DIR/test_relay_client"
