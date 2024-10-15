#!/usr/bin/env bash
set -Eeuo pipefail

if [[ $# -ne 1 ]]; then
    echo 'Usage: merge-debug-info.sh /path/to/binary'
    exit 1
fi

BINARY="$1"

# Grab the build ID of the binary
BUILD_ID="$(eu-unstrip -n -e "$BINARY" | awk '{print $2}' | cut -f1 -d@)"

# See https://sourceware.org/gdb/onlinedocs/gdb/Separate-Debug-Files.html
# For the “build ID” method, GDB looks in the .build-id subdirectory of each
# one of the global debug directories for a file named nn/nnnnnnnn.debug,
# where nn are the first 2 hex characters of the build ID bit string, and
# nnnnnnnn are the rest of the bit string. (Real build ID strings are 32 or
# more hex characters, not 10.)
DEBUG_FILE="/usr/lib/debug/.build-id/${BUILD_ID:0:2}/${BUILD_ID:2}.debug"

if [[ ! -f "${DEBUG_FILE}" ]]; then
    echo "Debug file '${DEBUG_FILE}' does not exist!"
    exit 2
fi

# Merge binary and debug info
eu-unstrip "${BINARY}" "${DEBUG_FILE}" -o "${BINARY}"
