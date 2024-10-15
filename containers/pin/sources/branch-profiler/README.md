# Branch Profiler Pin Plugin

## Description

This Pin tool profiles branches: for each branch, it returns how many times the branch was taken or not taken.

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

Use `pin -t libBranchProfiler.so -help -- ls` for a list of all available options.
Options that are specific to this Pin tool:

```
-csv_prefix  [default ]
	Set the prefix used for the CSV output file. The output file will be
	of the form <prefix>.branches.csv.
-end_after_main  [default 1]
	When true, ends analysis after main() is finished
-o  [default branchprofiler.log]
	Specify the filename of the human-readable log file
-start_from_main  [default 1]
	When true, starts analysis from the point that main() is called
```

## Output

The Pin tool outputs two files: a human readable log file, and a CSV file `<prefix>.branches.csv`.

### Human readable log file

This file contains a log of the execution of the Pin plugin.
It does not contain the branches that are found.

### `<prefix>.branches.csv`

This file is intended to be parsed by another application.

The `util/pretty-print-csvs.py` helper script can be used to print the contents of this CSV file in a human readable format.
This script is also used in the unit tests (`check` target).
For example:
```bash
util/pretty-print-csvs.py --prefix=path/to/prefix --log_file=path/to/branchprofiler.log
```
will print the contents of `path/to/branchprofiler.log` and `path/to/prefix.branches.csv`.

This CSV file contains the following fields:
- `image_name`: The filename of the image this branch belongs to.
- `image_offset`: The offset from the start of the image of this branch.
- `num_taken`: The number of times this branch was taken.
- `num_not_taken`: The number of times this branch was not taken.
