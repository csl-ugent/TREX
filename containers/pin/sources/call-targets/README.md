# Call Targets Pin Plugin

## Description

This Pin tool collects the targets of (indirect) call instructions.
This can be used to collect the (partial) groundtruth for the evaluation of call graph construction algorithms.

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

Use `pin -t libCallTargetsPinTool.so -help -- ls` for a list of all available options.
Options that are specific to this Pin tool:

```
-csv_prefix  [default ]
	Set the prefix used for the CSV output file. The output file will be
	of the form <prefix>.call-targets.csv.
-end_after_main  [default 1]
	When true, ends analysis after main() is finished
-o  [default calltargets.log]
	Specify the filename of the human-readable log file
-start_from_main  [default 1]
	When true, starts analysis from the point that main() is called
```

## Output

The Pin tool outputs a CSV file containing the found call targets: `<prefix>.call-targets.csv`.

### CSV files

The CSV files are intended to be parsed by another application.

The `util/pretty-print-csvs.py` helper script can be used to print the contents of the CSV files in a human readable format.
This script is used in the unit tests (`check` target).
For example:
```bash
util/pretty-print-csvs.py --prefix=path/to/prefix
```
will print the contents of `path/to/prefix.call-targets.csv`.

#### `<prefix>.call-targets.csv`

This file contains information about call targets.
Each line in the output file corresponds to a (call instruction = call site, call target = callee) pair.
Callees from the same call instruction are output after each other.

- `image_name`: The filename of the image of the call instruction.
- `image_offset`: The offset from the start of the image of the call instruction.
- `DEBUG_filename`: The filename of the source location of the call instruction, according to the debug information.
- `DEBUG_line`: The line number of the source location of the call instruction, according to the debug information.
- `DEBUG_column`: The column number of the source location of the call instruction, according to the debug information.
- `target_image_name`: The filename of the image of the call target.
- `target_image_offset`: The offset from the start of the image of the call target.
- `target_function_name`: The name of the function (i.e. routine in Pin terminology) of the call target.
- `DEBUG_target_filename`: The filename of the source location of the call target, according to the debug information.
- `DEBUG_target_line`: The line number of the source location of the call target, according to the debug information.
- `DEBUG_target_column`: The column number of the source location of the call target, according to the debug information.
