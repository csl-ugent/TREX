#!/usr/bin/env bash
set -Eeuo pipefail

CURL_VERSION="2cfac302fbeec68f1727cba3d1705e16f02220ad" # curl-7_68_0
OPENSSL_VERSION="a7222fc14d5977210d2b4673a68039824a039dc2" # OpenSSL_1_1_1-stable

# cd into current directory
cd "$( dirname "${BASH_SOURCE[0]}" )"

# logging
log()  { echo -e "\033[0;36m[LOG]\033[0m $1" >&2; }
warn() { echo -e "\033[0;33m[WRN]\033[0m $1" >&2; }
err()  { echo -e "\033[0;31m[ERR]\033[0m $1" >&2; }
die()  { err "$1"; exit 1; }

if [[ -z "${IS_RUNNING_IN_DOCKER+x}" ]]; then
    command -v docker &>/dev/null || die "Docker is not installed"

    log "Building docker image..."
    docker build . >/dev/null || die "Could not build docker image"
    IMAGE_ID="$(docker build -q .)"
    log "Successfully built image $IMAGE_ID."

    docker run --rm -it -v $PWD:/host $IMAGE_ID

    # Test if we can curl https
    log "Testing curl command on host..."
    LD_LIBRARY_PATH=. ./curl https://tools.ietf.org/rfc/rfc5246.txt -o /dev/null &>/dev/null || die "Could not run test command on host!"
else
    log "Running build script in Docker..."

    #############
    #  OpenSSL  #
    #############

    log "Cloning openssl ($OPENSSL_VERSION)..."
    git clone https://github.com/openssl/openssl
    pushd openssl
    git reset --hard $OPENSSL_VERSION

    log "Applying patches for openssl..."
    git apply /host/patches/openssl/*.patch

    log "Running configure for openssl..."
    ./config -g

    log "Compiling openssl..."
    make -j$(nproc)

    log "Installing openssl..."
    make install

    popd

    ##########
    #  curl  #
    ##########

    log "Cloning curl ($CURL_VERSION)..."
    git clone https://github.com/curl/curl
    pushd curl
    git reset --hard $CURL_VERSION

    log "Running configure for curl..."
    autoreconf -fi
    ./configure --prefix=/prefix --enable-debug --disable-curldebug --with-ssl=/usr/local

    log "Compiling curl..."
    make -j$(nproc)

    log "Installing curl..."
    make install

    popd

    #####################
    #  Copying to host  #
    #####################

    log "Copying binaries and libraries to host..."
    cp /prefix/bin/curl /host/curl

    # Copy shared libraries, ignoring vDSO, dynamic linker, libc, and libpthread
    # Note that we need to set LD_LIBRARY_PATH so that we use libcrypto and libssl from /usr/local instead of the system-wide one.
    LD_LIBRARY_PATH=/usr/local/lib ldd /prefix/bin/curl | grep -v -e '\<linux-vdso\>' -e '\<ld-linux\>' -e '\<libc\>' -e '\<libpthread\>' | awk '{print $3}' | xargs -I{} cp {} /host

    log "Testing curl command in Docker..."
    LD_LIBRARY_PATH=/host /host/curl https://tools.ietf.org/rfc/rfc5246.txt -o /dev/null &>/dev/null || die "Could not run test command in Docker!"

    #########################
    #  Checking debug info  #
    #########################

    log "Checking debug information..."
    file /host/curl | grep -q 'with debug_info' || die "curl does not have debug information!"
    file /host/libcurl* | grep -q 'with debug_info' || die "libcurl does not have debug information!"
    file /host/libcrypto* | grep -q 'with debug_info' || die "libcrypto does not have debug information!"
    file /host/libssl* | grep -q 'with debug_info' || die "libssl does not have debug information!"
fi
