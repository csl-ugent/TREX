#!/bin/bash
set -e

# /target contains target binary/disassembly
# /patterns contains pattern file
cd /target

echo "grap" "$@" > "cmd.out"
grap "$@"
