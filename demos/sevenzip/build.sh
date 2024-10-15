#!/usr/bin/env bash
set -Eeuo pipefail

P7ZIP_VERSION="16.02"
P7ZIP_SHA256="5eb20ac0e2944f6cb9c2d51dd6c4518941c185347d4089ea89087ffdd6e2341f"

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

    # Test if we can run 7za
    log "Testing 7za command on host..."
    ./7za -mmt1 -psecret a tmp.7z test.pdf &>/dev/null || die "Could not run test command on host!"
    rm tmp.7z
else
    log "Running build script in Docker..."

    ###########
    #  p7zip  #
    ###########

    log "Downloading source for p7zip ($P7ZIP_VERSION)..."
    wget https://sourceforge.net/projects/p7zip/files/p7zip/${P7ZIP_VERSION}/p7zip_${P7ZIP_VERSION}_src_all.tar.bz2/download -O p7zip_src.tar.bz2
    echo "${P7ZIP_SHA256} p7zip_src.tar.bz2" | sha256sum -c || die "Checksum of source doesn't match!"
    tar xvf p7zip_src.tar.bz2
    pushd p7zip_${P7ZIP_VERSION}

    log "Applying patches..."
    git apply /host/patches/*.patch

    log "Compiling p7zip..."
    cp makefile.linux_amd64 makefile.machine
    make all_test

    log "Installing p7zip..."
    mkdir -p /prefix/bin
    cp bin/7za /prefix/bin/7za

    popd

    #####################
    #  Copying to host  #
    #####################

    log "Copying binaries to host..."
    cp /prefix/bin/7za /host/7za

    log "Testing 7za command in Docker..."
    /host/7za -mmt1 -psecret a /tmp/test.7z /host/test.pdf &>/dev/null || die "Could not run test command in Docker!"

    log "Checking debug information..."
    file /host/7za | grep -q 'with debug_info' || die "7za does not have debug information!"
fi
