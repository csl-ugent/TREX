#!/usr/bin/env bash
set -Eeuo pipefail

# RUN: rm -f %t.7z
# RUN: %sde -log -log:mt -log:basename %t/pinball -- %S/benchmarks/7za-x64 -mmt1 -psecret a %t.7z %S/benchmarks/one-byte-file
# RUN: %sde %toolarg -output %t.log -replay -replay:basename %t/pinball -replay:addr_trans -- nullapp

# XFAIL: x86
