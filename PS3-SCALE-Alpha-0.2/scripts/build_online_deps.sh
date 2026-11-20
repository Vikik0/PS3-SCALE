#!/bin/sh
set -eu

: "${PS3DEV:?PS3DEV must point to the PS3 toolchain}"
: "${PSL1GHT:?PSL1GHT must point to PSL1GHT}"

PORTLIBS="${PORTLIBS:-$PS3DEV/portlibs/ppu}"
export PORTLIBS
TOOLCHAIN="$PS3DEV/ppu/bin"
PREFIX="$TOOLCHAIN/powerpc64-ps3-elf-"
WORK="${RUNNER_TEMP:-/tmp}/ps3scale-https-deps"
PROJECT_ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
CA_BUNDLE_RUNTIME_PATH="/dev_hdd0/game/PS3SCALE1/USRDIR/cacert.pem"

# curl's configure probe checks the runtime path even though the bundle is
# packaged later by Makefile. Stage the exact bundled CA file at that path
# inside the build container so the probe and the shipped runtime agree.
mkdir -p "$(dirname -- "$CA_BUNDLE_RUNTIME_PATH")"
cp "$PROJECT_ROOT/pkgfiles/USRDIR/cacert.pem" "$CA_BUNDLE_RUNTIME_PATH"
test -s "$CA_BUNDLE_RUNTIME_PATH"

mkdir -p "$PORTLIBS/include" "$PORTLIBS/lib" "$WORK"
export PATH="$TOOLCHAIN:$PATH"

# Build the Apache-2.0 option of Mbed TLS for the PPU target.
PS3LIBRARIES_COMMIT="74095b14f6327420787ca2f7fe95b0c58c835cc4"
git init "$WORK/ps3libraries"
git -C "$WORK/ps3libraries" remote add origin https://github.com/bucanero/ps3libraries.git
git -C "$WORK/ps3libraries" fetch --depth 1 origin "$PS3LIBRARIES_COMMIT"
git -C "$WORK/ps3libraries" checkout --detach FETCH_HEAD
test "$(git -C "$WORK/ps3libraries" rev-parse HEAD)" = "$PS3LIBRARIES_COMMIT"
cd "$WORK"
wget -q -O mbedtls-2.28.10.tar.bz2 \
  https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-2.28.10/mbedtls-2.28.10.tar.bz2
tar -xjf mbedtls-2.28.10.tar.bz2
cd mbedtls-2.28.10
patch -p1 < "$WORK/ps3libraries/patches/mbedtls-2.28.10.patch"

export CC="${PREFIX}gcc"
export LD="${PREFIX}ld"
export CPP="${PREFIX}cpp"
export CXX="${PREFIX}g++"
export AR="${PREFIX}ar"
export AS="${PREFIX}as"
export NM="${PREFIX}nm"
export RANLIB="${PREFIX}ranlib"
export CFLAGS="-O2 -mcpu=cell -I$PS3DEV/ppu/powerpc64-ps3-elf/include -I$PSL1GHT/ppu/include -I$PORTLIBS/include"
export LDFLAGS="-L$PS3DEV/ppu/powerpc64-ps3-elf/lib -L$PSL1GHT/ppu/lib -L$PORTLIBS/lib -lrt -llv2"

make -C library
cp library/libmbed*.a "$PORTLIBS/lib/"
cp -R include/mbedtls "$PORTLIBS/include/"
cp LICENSE "$PORTLIBS/include/mbedtls/LICENSE"

# Build a small static libcurl with Mbed TLS; no PolarSSL/GPL linkage.
cd "$WORK"
wget -q -O curl-7.64.1.tar.gz https://curl.se/download/curl-7.64.1.tar.gz
tar -xzf curl-7.64.1.tar.gz
cd curl-7.64.1

export CPPFLAGS="-I$PORTLIBS/include -I$PSL1GHT/ppu/include"
export CFLAGS="-O2 -mcpu=cell -I$PORTLIBS/include -I$PSL1GHT/ppu/include"
# Keep the PPU sysroot libraries visible to configure's link probes. The
# socket resolver lives in the PSL1GHT/PS3 net libraries, not in portlibs.
export LDFLAGS="-L$PS3DEV/ppu/powerpc64-ps3-elf/lib -L$PS3DEV/ppu/lib -L$PSL1GHT/ppu/lib -L$PORTLIBS/lib"
# Keep static dependencies after the network archives so configure's link probes
# resolve sysModuleLoad/sysModuleUnload pulled in by libnet.a.
export LIBS="-lnetctl -lnet -lsysmodule -llv2 -lrt"
if ! ./configure \
  --host=powerpc64-ps3-elf \
  --prefix="$PORTLIBS" \
  --disable-shared --enable-static \
  --disable-threaded-resolver --disable-ipv6 \
  --disable-ntlm --disable-ntlm-wb \
  --disable-ldap --disable-ldaps --disable-ftp --disable-file \
  --disable-rtsp --disable-dict --disable-telnet --disable-tftp \
  --disable-pop3 --disable-imap --disable-smtp --disable-gopher --disable-smb \
  --with-mbedtls="$PORTLIBS" \
  --with-ca-bundle="/dev_hdd0/game/PS3SCALE1/USRDIR/cacert.pem"; then
  echo "=== curl resolver configure diagnostics ==="
  grep -n -B 8 -A 24 "gethostbyname" config.log || true
  echo "=== available PPU network libraries ==="
  find "$PS3DEV/ppu" "$PORTLIBS" -type f \\
    \( -name 'libnet*.a' -o -name 'libnet*.so' -o -name 'libnetctl*.a' \) -print
  for library in $(find "$PS3DEV/ppu" "$PORTLIBS" -type f \\
      \( -name 'libnet*.a' -o -name 'libnetctl*.a' \)); do
    echo "--- $library"
    "$PREFIX"nm -g "$library" 2>/dev/null | grep -i "gethostbyname" || true
  done
  exit 1
fi
make -j2
make install

test -s "$PORTLIBS/lib/libcurl.a"
test -s "$PORTLIBS/lib/libmbedtls.a"
test -s "$PORTLIBS/lib/libmbedx509.a"
test -s "$PORTLIBS/lib/libmbedcrypto.a"
test -s "$PORTLIBS/include/curl/curl.h"
echo "PS3 SCALE HTTPS dependencies ready (libcurl + Mbed TLS)."
