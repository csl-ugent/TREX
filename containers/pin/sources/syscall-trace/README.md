# System Call Trace Pin Plugin

## Description

This Pin tool collects the system calls executed by the process.
The arguments and return value of the system calls are printed in a human-readable fashion, e.g. pointers are printed as hexadecimal values, (un)signed values are printed as such, etc.
It is thus similar to `strace`, but is able to be used on a pinball, hence increasing reproducibility.

## Compilation

```bash
mkdir build && cd build
cmake -DPin_ROOT_DIR=~/bin/pin-3.16-98275-ge0db48c31-gcc-linux/ ..
cmake --build .
```

**NOTE:** Make sure you use `g++` and not `clang++`!

You can also specify `-DPin_TARGET_ARCH=x86` to compile a 32-bit version of the plugin.

## Testing

You can run the unit tests using:

```bash
cd build/
cmake --build . --target check
```

## Options

Use `sde64 -t64 libSyscallTrace.so -help-long` for a list of all available options.
Options that are specific to this Pin tool:

```
-csv_prefix  [default ]
	Set the prefix used for the CSV output file. The output file will be
	of the form <prefix>.system-calls.csv.
-output  [default systemcalls.log]
	Specify the filename of the human-readable log file
```

## Output

The Pin tool outputs two files: a human readable log file, and a CSV file `<prefix>.system-calls.csv`.

### Human readable log file

This file contains a log of the execution of the Pin plugin.
It does not contain the resulting strace.

### `<prefix>.system-calls.csv`

This file is intended to be parsed by another application.

The `util/pretty-print-csvs.py` helper script can be used to print the contents of this CSV file in a human readable format.
This script is also used in the unit tests (`check` target).
For example:
```bash
util/pretty-print-csvs.py --prefix=path/to/prefix --log_file=path/to/systemcalls.log
```
will print the contents of `path/to/systemcalls.log` and `path/to/prefix.system-calls.csv`.

This CSV file contains the following fields:
- `name`: A human readable name of the system call, e.g. `mmap`.
- `num_arguments`: The number of arguments of this system call.
- `argument_{N}`: The `N`th argument of this system call.
- `return_value`: The return value of this system call.
- `thread_id`: The ID of the thread that invoked the system call.
- `syscall_id`: The ID of the system call within its thread.
