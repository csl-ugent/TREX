# Instruction Info Pin Plugin

## Description

This Pin tool collects some information for each instruction that is executed: which image and routine it is in, its opcode, operands, etc.

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

Use `pin -t libInstructionInfo.so -help -- ls` for a list of all available options.
Options that are specific to this Pin tool:

```
-csv_prefix  [default ]
	Set the prefix used for the CSV output file. The output file will be
	of the form <prefix>.instruction-info.csv.
-o  [default instructioninfo.log]
	Specify the filename of the human-readable log file
```

## Output

The Pin tool outputs two files: a human readable log file, and a CSV file `<prefix>.instruction-info.csv`.

### Human readable log file

This file contains a log of the execution of the Pin plugin.
It does not contain the collected information of the instructions.

### `<prefix>.instruction-info.csv`

This file is intended to be parsed by another application.

The `util/pretty-print-csvs.py` helper script can be used to print the contents of this CSV file in a human readable format.
This script is also used in the unit tests (`check` target).
For example:
```bash
util/pretty-print-csvs.py --prefix=path/to/prefix --log_file=path/to/instructioninfo.log
```
will print the contents of `path/to/instructioninfo.log` and `path/to/prefix.instruction-info.csv`.

This CSV file contains the following fields for each instruction:

- `image_name`: the filename of the image this instruction belongs to
- `full_image_name`: the full path of the image this instruction belongs to
- `section_name`: the name of the section this instruction belongs to
- `image_offset`: the offset from the start of the image of this instruction
- `routine_name`: the name of the routine this instruction belongs to
- `routine_offset`: the offset from the start of the routine of this instruction
- `opcode`: the opcode of the instruction
- `operands`: a string representing the operands of the instruction
- `category`: the XED category of the instruction. The category is a higher level semantic description of an instruction than its opcode. See `xed-category-enum.h` for the possible values and `idata.txt` for the full mapping of opcodes to categories. Both these files are distributed as part of the Pin toolkit.
- `DEBUG_filename`: The filename of the source location this instruction corresponds to, according to the debug information.
- `DEBUG_line`: The line number of the source location this instruction corresponds to, according to the debug information.
- `DEBUG_column`: The column number of the source location this instruction corresponds to, according to the debug information.
- `bytes`: the bytes that form the instruction