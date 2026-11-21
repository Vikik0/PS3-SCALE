#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP_ROOT="$(printenv TMPDIR 2>/dev/null || true)"
if [[ -z "$TMP_ROOT" ]]; then TMP_ROOT="/tmp"; fi
BUILD_DIR="$TMP_ROOT/crash-bash-ws-test-$$"
WS_SERVER_PID=""
RELAY_SERVER_PID=""

cleanup() {
  if [[ -n "$WS_SERVER_PID" ]]; then kill "$WS_SERVER_PID" 2>/dev/null || true; wait "$WS_SERVER_PID" 2>/dev/null || true; fi
  if [[ -n "$RELAY_SERVER_PID" ]]; then kill "$RELAY_SERVER_PID" 2>/dev/null || true; wait "$RELAY_SERVER_PID" 2>/dev/null || true; fi
  rm -rf "$BUILD_DIR"
}
trap cleanup EXIT
mkdir -p "$BUILD_DIR"

MBEDTLS_SOURCE_DIR="$BUILD_DIR/mbedtls-src"
MBEDTLS_BUILD_DIR="$BUILD_DIR/mbedtls-host"
git clone --depth 1 --branch v2.28.10 https://github.com/Mbed-TLS/mbedtls.git "$MBEDTLS_SOURCE_DIR"
CC=gcc AR=ar RANLIB=ranlib CFLAGS="-O2 -fno-stack-protector" bash "$ROOT/build-mbedtls.sh" "$MBEDTLS_SOURCE_DIR" "$MBEDTLS_BUILD_DIR"

openssl req -x509 -newkey rsa:2048 -sha256 -nodes -days 2 -keyout "$BUILD_DIR/ca.key" -out "$BUILD_DIR/ca.pem" -subj "/CN=Crash Bash Local Test CA" -addext "basicConstraints=critical,CA:TRUE" -addext "keyUsage=critical,keyCertSign,cRLSign"
openssl req -newkey rsa:2048 -sha256 -nodes -keyout "$BUILD_DIR/server.key" -out "$BUILD_DIR/server.csr" -subj "/CN=localhost"

cat > "$BUILD_DIR/server.ext" <<'EOF'
basicConstraints=critical,CA:FALSE
keyUsage=critical,digitalSignature,keyEncipherment
extendedKeyUsage=serverAuth
subjectAltName=DNS:localhost
EOF

openssl x509 -req -in "$BUILD_DIR/server.csr" -CA "$BUILD_DIR/ca.pem" -CAkey "$BUILD_DIR/ca.key" -CAcreateserial -out "$BUILD_DIR/server.pem" -days 2 -sha256 -extfile "$BUILD_DIR/server.ext"
python3 "$ROOT/tools/embed_ca_bundle.py" "$BUILD_DIR/ca.pem" "$BUILD_DIR/ca_bundle.c"

gcc -std=c11 -D_POSIX_C_SOURCE=200112L -Wall -Wextra -Werror -DMBEDTLS_CONFIG_FILE='"cb_mbedtls_config.h"' -I"$ROOT" -I"$MBEDTLS_BUILD_DIR/include" "$ROOT/ws_client.c" "$ROOT/tls_client.c" "$BUILD_DIR/ca_bundle.c" "$ROOT/tests/test_ws_client.c" "$MBEDTLS_BUILD_DIR/libcb_mbedtls.a" -lm -o "$BUILD_DIR/test_ws_client"
gcc -std=c11 -D_POSIX_C_SOURCE=200112L -Wall -Wextra -Werror -DMBEDTLS_CONFIG_FILE='"cb_mbedtls_config.h"' -I"$ROOT" -I"$MBEDTLS_BUILD_DIR/include" "$ROOT/ws_client.c" "$ROOT/relay_client.c" "$ROOT/tls_client.c" "$BUILD_DIR/ca_bundle.c" "$ROOT/tests/test_relay_client.c" "$MBEDTLS_BUILD_DIR/libcb_mbedtls.a" -lm -o "$BUILD_DIR/test_relay_client"

CB_TEST_TLS_CERT="$BUILD_DIR/server.pem" CB_TEST_TLS_KEY="$BUILD_DIR/server.key" node "$ROOT/tests/ws_server.mjs" >"$BUILD_DIR/ws-server.log" 2>&1 &
WS_SERVER_PID="$!"
CB_TEST_TLS_CERT="$BUILD_DIR/server.pem" CB_TEST_TLS_KEY="$BUILD_DIR/server.key" node "$ROOT/tests/relay_server.mjs" >"$BUILD_DIR/relay-server.log" 2>&1 &
RELAY_SERVER_PID="$!"

for _ in $(seq 1 80); do
  if grep -q "ready" "$BUILD_DIR/ws-server.log" && grep -q "ready" "$BUILD_DIR/relay-server.log"; then break; fi
  sleep 0.1
done
if ! grep -q "ready" "$BUILD_DIR/ws-server.log" || ! grep -q "ready" "$BUILD_DIR/relay-server.log"; then
  cat "$BUILD_DIR/ws-server.log" "$BUILD_DIR/relay-server.log"
  echo "WebSocket/TLS test servers did not start." >&2
  exit 1
fi

"$BUILD_DIR/test_ws_client"
"$BUILD_DIR/test_relay_client"
