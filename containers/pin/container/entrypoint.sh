#!/bin/bash
set -e

SDE_COMMAND=$1
TIMEOUT=$2
TARGET_BINARY=$3
CHOWN_TO=$4
TUNABLES=$5
INTERPOSER=$6
EXTRA_COMMAND=$7

# /pin contains pin installation
# /target contains target binary, acts as a portal to funnel back output
# /compiled_tools contains target pintool
cd /target
chmod +x "${TARGET_BINARY}"

# Run init.sh if it exists.
[[ -f "init.sh" ]] && chmod +x init.sh && echo "Running init.sh..." >&2 && ./init.sh

# allow the executable to load shared libraries from the directory of the executable (i.e. /target)
export LD_LIBRARY_PATH=.
# if tunables are present, set them
if [[ -n "${TUNABLES}" ]]; then
  export GLIBC_TUNABLES="${TUNABLES}"
fi
# if an interposer is present, set it
if [[ -n "${INTERPOSER}" ]]; then
  gcc -O3 -shared -fno-builtin -fPIC -mno-avx -mno-sse "${INTERPOSER}" -o /target/interposer.so -ldl
  export LD_PRELOAD=/target/interposer.so
fi

# Combination of:
# - timeout command to kill the process after a timed interval to prevent too large traces
# - sde to record/replay the actual trace
# - redirect all output to stdout, so we can inspect it when a ContainerError is raised
# shellcheck disable=SC2086
timeout --preserve-status $TIMEOUT $SDE_COMMAND

# if tunables are present, unset them
if [[ -n "${TUNABLES}" ]]; then
  export -n GLIBC_TUNABLES
fi
# if an interposer is present, unset it
if [[ -n "${INTERPOSER}" ]]; then
  export -n LD_PRELOAD
fi

if [[ -n "${EXTRA_COMMAND}" ]]; then
    bash -c "${EXTRA_COMMAND}"
fi

# Merge debug info for ELF files, but only after running SDE,
# since otherwise SDE does not find malloc for some reason...
/merge-debug-info.sh /lib/i386-linux-gnu/libc.so.6
/merge-debug-info.sh /lib/x86_64-linux-gnu/libc.so.6

# Patch all CSV files
find /target -name '*.csv' | xargs -I{} sh -c '/patch-debug-info.py {} -o {}.patched && mv {}.patched {}'
chown -R "$CHOWN_TO" /target
