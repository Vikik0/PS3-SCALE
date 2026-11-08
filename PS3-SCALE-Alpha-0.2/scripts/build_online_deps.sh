#!/bin/sh
set -eu

: "${PS3DEV:?PS3DEV must point to the PS3 toolchain}"
: "${PSL1GHT:?PSL1GHT must point to PSL1GHT}"

PORTLIBS="${PORTLIBS:-$PS3DEV/portlibs/ppu}"
export PORTLIBS
TOOLCHAIN="$PS3DEV/ppu/bin"
PREFIX="$TOOLCHAIN/powerpc64-ps3-elf-"
WORK="${RUNNER_TEMP:-/tmp}/ps3scale-https-deps"

mkdir -p "$PORTLIBS/include" "$PORTLIBS/lib" "$WORK"
export PATH="$TOOLCHAIN:$PATH"

# Build the Apache-2.0 option of Mbed TLS for the PPU target.
git clone --depth 1 https://github.com/bucanero/ps3libraries.git "$WORK/ps3libraries"
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
export LDFLAGS="-L$PORTLIBS/lib -L$PSL1GHT/ppu/lib -lrt -llv2"
./configure \
  --host=powerpc64-ps3-elf \
  --prefix="$PORTLIBS" \
  --disable-shared --enable-static \
  --disable-threaded-resolver --disable-ipv6 \
  --disable-ldap --disable-ldaps --disable-ftp --disable-file \
  --disable-rtsp --disable-dict --disable-telnet --disable-tftp \
  --disable-pop3 --disable-imap --disable-smtp --disable-gopher --disable-smb \
  --with-mbedtls="$PORTLIBS" \
  --with-ca-bundle="/usr/ssl/certs/ca-bundle.crt"
make -j2
make install

test -s "$PORTLIBS/lib/libcurl.a"
test -s "$PORTLIBS/lib/libmbedtls.a"
test -s "$PORTLIBS/lib/libmbedx509.a"
test -s "$PORTLIBS/lib/libmbedcrypto.a"
test -s "$PORTLIBS/include/curl/curl.h"
echo "PS3 SCALE HTTPS dependencies ready (libcurl + Mbed TLS)."
