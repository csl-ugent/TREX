#!/bin/bash
set -e

# if this changes, also update Dockerfile!
deobfuscator_folder="generic_deobfuscator"

# /target contains trace
cd /target

# The original version of generic deobfuscator segfaults if the files folder is missing, so just add it for certainty (for the -p option)
if [ ! -d files ]; then
  mkdir files
fi
echo "/${deobfuscator_folder}/deobf" "$@" > "cmd.out"
/${deobfuscator_folder}/deobf "$@"
