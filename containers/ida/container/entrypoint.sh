#!/bin/bash
set -e

cd /target

if [ "${3:-False}" == "False" ]; then
  /ida/ida_entry -a- -A -L/target/bz.ida.log -S/plugins/"$1" /target/"$2"
else
  /ida/ida_entry64 -a- -A -L/target/bz.ida.log -S/plugins/"$1" /target/"$2"
fi