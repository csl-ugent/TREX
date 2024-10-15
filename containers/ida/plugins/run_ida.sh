#!/usr/bin/env bash
# $0 <executable path and name>

root_dir=$(cd $(dirname $0) && pwd)
binary=$1
shift
args="$@"

rm -f $binary.ida.log
${HOME}/ida-7.1/idat -c -a- -A -L$binary.ida.log -S"$root_dir/ida_plugin.py $args" $binary

# error code checking
if [[ $? -ne 0 ]]; then
  echo "ERROR $binary.ida.log"
else
  echo "Log: $binary.ida.log"
fi

#./idat -c -a- -A -Lbz.ida.log -S/plugins/ida_plugin.py /target/a.out