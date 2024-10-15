#!/bin/bash
set -e

arch="$1"
name="$2"

pin_arch="x64"
if [ "$arch" == "ia32" ]; then
  pin_arch="x86"
fi

if [ -d /plugin-build ]; then
  rm -rf /plugin-build
fi
mkdir /plugin-build
cd /plugin-build

cmake -DPin_ROOT_DIR=/pin -DPin_TARGET_ARCH=${pin_arch} "/source_tools/${name}"
cmake --build .

mv lib*.so "/compiled_tools/${arch}_${name}.so"
