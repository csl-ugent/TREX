#!/usr/bin/env bash
set -Eeuo pipefail

# RUN: %sde -log -log:basename %t/pinball -- %s
# RUN: %sde %toolarg -output %t.log -replay -replay:basename %t/pinball -replay:addr_trans -- nullapp

# XFAIL: x86

# Just check that the tool doesn't crash with openssl.
openssl aes-256-cbc -in <(for i in {1..100}; do echo "foobarbaz"; done) -out test.out -pass pass:mypassword
