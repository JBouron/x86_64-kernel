#!/bin/bash

set -e

# Builds GCC and binutils from sources within the docker container. Meant to be
# called from the Dockerfile.
# A couple of assumptions:
#   - GCC's sources are located under /build/gcc-sources
#   - Binutils's sources are located under /build/binutils-sources

GCC_SOURCES_DIR=/build/gcc-sources
BINUTILS_SOURCES_DIR=/build/binutils-sources

# A few env variables used by both build processes:
# Since we are installing the cross-compiler in a docker container we can simply
# install it system-wide.
export PREFIX=/usr/local/cross
# Target x86_64
export TARGET=x86_64-elf
export PATH="$PREFIX/bin:$PATH"

# Get the number of CPUS for the number of make jobs.
NCPUS=$(lscpu | grep "^CPU(s):" | awk '{print $2}')

# Start with binutils for no other reason that the osdev tutorial buils binutils
# first.
mkdir /build/binutils-build && cd /build/binutils-build
../binutils-sources/configure \
    --target="$TARGET" \
    --prefix="$PREFIX" \
    --with-sysroot \
    --disable-nls \
    --disable-werror
make -j$NCPUS
make -j$NCPUS install


# Now build GCC.
mkdir /build/gcc-build && cd /build/gcc-build

# The $PREFIX/bin dir _must_ be in the PATH. We did that above.
which -- $TARGET-as || echo $TARGET-as is not in the PATH

../gcc-sources/configure \
    --target="$TARGET" \
    --prefix="$PREFIX" \
    --disable-nls \
    --enable-languages=c,c++ \
    --without-headers
make -j$NCPUS all-gcc
# Note: Linking the kernel in higher-half requires compiling with mcmodel=large.
# However libgcc does not support mcmodel=large by default, it needs to be
# compiled with it first.
# Also enable no-red-zone with libgcc to avoid surprises.
make -j$NCPUS all-target-libgcc \
    CFLAGS_FOR_TARGET='-g -O2 -mcmodel=large -mno-red-zone'
make -j$NCPUS install-gcc
make -j$NCPUS install-target-libgcc
