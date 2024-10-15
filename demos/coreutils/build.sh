#!/usr/bin/env bash
set -Eeuo pipefail

COREUTILS_VERSION="9.1"
COREUTILS_SHA256="61a1f410d78ba7e7f37a5a4f50e6d1320aca33375484a3255eddf17a38580423"

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
else
    log "Running build script in Docker..."

    log "Cloning coreutils $COREUTILS_VERSION..."
    wget https://ftp.gnu.org/gnu/coreutils/coreutils-$COREUTILS_VERSION.tar.xz -O coreutils.tar.xz
    echo "${COREUTILS_SHA256} coreutils.tar.xz" | sha256sum -c || die "Checksum of source doesn't match!"
    tar xvf coreutils.tar.xz
    pushd coreutils-${COREUTILS_VERSION}

    log "Compiling coreutils..."
    FORCE_UNSAFE_CONFIGURE=1 ./configure
    make

    log "Installing coreutils..."
    make install

    popd

    log "Copying binaries to the host..."
    find /usr/local -type f -executable -exec cp '{}' /host \;
fi
