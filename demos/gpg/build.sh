#!/usr/bin/env bash
set -Eeuo pipefail

ZLIB_VERSION="1.2.11"
ZLIB_SHA256="c3e5e9fdd5004dcb542feda5ee4f0ff0744628baf8ed2dd5d66f8ca1197cb1a1"

BZIP2_VERSION="6a8690fc8d26c815e798c588f796eabe9d684cf0" # 1.0.8

NPTH_VERSION="1.6"
NPTH_SHA256="1393abd9adcf0762d34798dc34fdcf4d0d22a8410721e76f1e3afcd1daa4e2d1"

LIBGPG_ERROR_VERSION="1.42"
LIBGPG_ERROR_SHA256="fc07e70f6c615f8c4f590a8e37a9b8dd2e2ca1e9408f8e60459c67452b925e23"

LIBGCRYPT_VERSION="1.8.8" # latest LTS release
LIBGCRYPT_SHA256="895de2bb981dd127f0821d1ce13fadf7d760f9fa7737648b15f2c1fe13cc5af5"

LIBKSBA_VERSION="1.6.0"
LIBKSBA_SHA256="dad683e6f2d915d880aa4bed5cea9a115690b8935b78a1bbe01669189307a48b"

LIBASSUAN_VERSION="2.5.5"
LIBASSUAN_SHA256="8e8c2fcc982f9ca67dcbb1d95e2dc746b1739a4668bc20b3a3c5be632edb34e4"

GNUPG_VERSION="2.2.29" # latest LTS release
GNUPG_SHA256="39d07cdb4524818f9ebce49294931974af504519e6a7476c52e9d38fc0bd0cc9"

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

    # Test if we can run gpg
    log "Testing gpg command on host..."
    echo "Horum omnium fortissimi sunt Belgae" > input
    LD_LIBRARY_PATH=$PWD ./gpg --output encrypted --symmetric --cipher-algo AES256 --passphrase secret --batch input
    LD_LIBRARY_PATH=$PWD ./gpg --output decrypted --decrypt --passphrase secret --batch encrypted
    cmp input decrypted
    rm input encrypted decrypted
else
    log "Running build script in Docker..."

    ##########
    #  zlib  #
    ##########

    log "Downloading source for zlib ($ZLIB_VERSION)..."
    wget https://zlib.net/fossils/zlib-${ZLIB_VERSION}.tar.gz -O zlib-src.tar.gz
    echo "${ZLIB_SHA256} zlib-src.tar.gz" | sha256sum -c || die "Checksum of source doesn't match!"
    tar xvzf zlib-src.tar.gz
    pushd zlib-${ZLIB_VERSION}

    log "Compiling zlib..."
    CFLAGS="-g" ./configure
    make test

    log "Installing zlib..."
    make install

    popd

    ###########
    #  bzip2  #
    ###########

    log "Downloading source for bzip2 ($BZIP2_VERSION)..."
    git clone git://sourceware.org/git/bzip2.git
    pushd bzip2

    log "Compiling bzip2..."
    make

    log "Installing bzip2..."
    make install

    popd

    ##########
    #  nPth  #
    ##########

    log "Downloading source for nPth ($NPTH_VERSION)..."
    wget https://gnupg.org/ftp/gcrypt/npth/npth-${NPTH_VERSION}.tar.bz2 -O npth-src.tar.bz2
    echo "${NPTH_SHA256} npth-src.tar.bz2" | sha256sum -c || die "Checksum of source doesn't match!"
    tar xvf npth-src.tar.bz2
    pushd npth-${NPTH_VERSION}

    log "Compiling nPth..."
    ./configure
    make
    make check

    log "Installing nPth..."
    make install

    popd

    ##################
    #  libgpg-error  #
    ##################

    log "Downloading source for libgpg-error ($LIBGPG_ERROR_VERSION)..."
    wget https://gnupg.org/ftp/gcrypt/libgpg-error/libgpg-error-${LIBGPG_ERROR_VERSION}.tar.bz2 -O libgpg-error-src.tar.bz2
    echo "${LIBGPG_ERROR_SHA256} libgpg-error-src.tar.bz2" | sha256sum -c || die "Checksum of source doesn't match!"
    tar xvf libgpg-error-src.tar.bz2
    pushd libgpg-error-${LIBGPG_ERROR_VERSION}

    log "Compiling libgpg-error..."
    ./configure
    make
    make check

    log "Installing libgpg-error..."
    make install

    popd

    ###############
    #  libgcrypt  #
    ###############

    log "Downloading source for libgcrypt ($LIBGCRYPT_VERSION)..."
    wget https://gnupg.org/ftp/gcrypt/libgcrypt/libgcrypt-${LIBGCRYPT_VERSION}.tar.bz2 -O libgcrypt-src.tar.bz2
    echo "${LIBGCRYPT_SHA256} libgcrypt-src.tar.bz2" | sha256sum -c || die "Checksum of source doesn't match!"
    tar xvf libgcrypt-src.tar.bz2

    pushd libgcrypt-${LIBGCRYPT_VERSION}

    log "Compiling libgcrypt..."
    ./configure --disable-aesni-support
    make
    make check

    log "Installing libgcrypt..."
    make install

    popd

    #############
    #  libksba  #
    #############

    log "Downloading source for libksba ($LIBKSBA_VERSION)..."
    wget https://gnupg.org/ftp/gcrypt/libksba/libksba-${LIBKSBA_VERSION}.tar.bz2 -O libksba-src.tar.bz2
    echo "${LIBKSBA_SHA256} libksba-src.tar.bz2" | sha256sum -c || die "Checksum of source doesn't match!"
    tar xvf libksba-src.tar.bz2
    pushd libksba-${LIBKSBA_VERSION}

    log "Compiling libksba..."
    ./configure
    make
    make check

    log "Installing libksba..."
    make install

    popd

    ###############
    #  libassuan  #
    ###############

    log "Downloading source for libassuan ($LIBASSUAN_VERSION)..."
    wget https://gnupg.org/ftp/gcrypt/libassuan/libassuan-${LIBASSUAN_VERSION}.tar.bz2 -O libassuan-src.tar.bz2
    echo "${LIBASSUAN_SHA256} libassuan-src.tar.bz2" | sha256sum -c || die "Checksum of source doesn't match!"
    tar xvf libassuan-src.tar.bz2
    pushd libassuan-${LIBASSUAN_VERSION}

    log "Compiling libassuan..."
    ./configure
    make
    make check

    log "Installing libassuan..."
    make install

    popd

    ###########
    #  GnuPG  #
    ###########

    log "Downloading source for GnuPG ($GNUPG_VERSION)..."
    wget https://gnupg.org/ftp/gcrypt/gnupg/gnupg-${GNUPG_VERSION}.tar.bz2 -O gnupg-src.tar.bz2
    echo "${GNUPG_SHA256} gnupg-src.tar.bz2" | sha256sum -c || die "Checksum of source doesn't match!"
    tar xvf gnupg-src.tar.bz2
    pushd gnupg-${GNUPG_VERSION}

    log "Compiling GnuPG..."
    ./configure --enable-all-tests
    make

    # Make sure that 'make check' uses the compiled versions in /usr/local/lib, and not
    # those in /usr/lib.
    LD_LIBRARY_PATH=/usr/local/lib make check

    log "Installing GnuPG..."
    make install

    popd

    #####################
    #  Copying to host  #
    #####################

    LIBRARIES="$(LD_LIBRARY_PATH=/usr/local/lib ldd /usr/local/bin/gpg | grep -v -e '\<linux-vdso\>' -e '\<ld-linux\>' -e '\<libc\>' -e '\<libpthread\>' | awk '{print $3}')"

    log "Copying binaries and libraries to host..."

    cp "/usr/local/bin/gpg" /host/
    cp "/usr/local/bin/gpg-agent" /host/

    for library in $LIBRARIES; do
        cp "${library}" /host/
    done

    log "Testing gpg command in Docker..."
    echo "Horum omnium fortissimi sunt Belgae" > /tmp/input
    LD_LIBRARY_PATH=/host /host/gpg --output /tmp/encrypted --symmetric --cipher-algo AES256 --passphrase secret --batch /tmp/input
    LD_LIBRARY_PATH=/host /host/gpg --output /tmp/decrypted --decrypt --passphrase secret --batch /tmp/encrypted
    cmp /tmp/input /tmp/decrypted

    #########################
    #  Checking debug info  #
    #########################

    log "Checking debug information..."

    file "/host/gpg" | grep -q 'with debug_info' || die "$binary does not have debug information!"

    for library in $LIBRARIES; do
        base="$(basename "$library")"
        file "/host/$base" | grep -q 'with debug_info' || die "$base does not have debug information!"
    done
fi
