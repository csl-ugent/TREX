# Instruction Values Pin Plugin

## Description

This Pin tool collects the values of register and memory operands of an instruction at run time.

Both read and written values are recorded.
The read values are captured _before_ the instruction, whereas the written values are captured _after_ the instruction.

Note that the values for each operand are interpreted as a set, i.e. duplicate values only occur once.

All values are recorded by default.
You can change this using the `-instruction_values_limit` command-line parameter, to make the resulting CSV file smaller.

By default, the entire executable is traced.
However, this analysis can be quite expensive performance-wise, so this Pintool supports only monitoring specific ranges of instructions using the `-range_image`, `-range_begin_offset`, and `-range_end_offset` command-line parameters.

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

Use `sde64 -t64 libInstructionValues.so -help-long` for a list of all available options.
Options that are specific to this Pin tool:

```
-csv_prefix  [default ]
	Set the prefix used for the CSV output file. The output file will be
	of the form <prefix>.instruction-values.csv.
-end_after_main  [default 1]
	When true, ends analysis after main() is finished
-instruction_values_limit  [default -1]
	The maximum number of unique read/written values stored per
	instruction operand. Set to '-1' to store ALL values.
-output  [default instructionvalues.log]
	Specify the filename of the human-readable log file
-range_begin_offset
	The offset of the beginning instruction of the instruction range. Note
	that you can repeat this option multiple times to specify multiple
	ranges. Also see: -range_image and -range_end_offset.
-range_end_offset
	The offset of the beyond-the-end instruction of the instruction range.
	Note that you can repeat this option multiple times to specify
	multiple ranges. Also see: -range_image and -range_begin_offset.
-range_image
	The name of the image of the instruction range. Both the beginning and
	end of the range are instructions in this image. Note that you can
	repeat this option multiple times to specify multiple ranges. Also
	see: -range_begin_offset and -range_end_offset.
-ranges_file  [default ]
	Set the file containing the instruction ranges to collect data for.
	Each line in this file represents a range and should contain the image
	name, begin offset, and range offset separated by whitespace. This
	option is an alternative for -range_image, -range_begin_offset, and
	-range_end_offset when you want to specify more instruction ranges
	than allowed by the maximum argument length.
-start_from_main  [default 1]
	When true, starts analysis from the point that main() is called
```

## Output

The Pin tool outputs two files: a human readable log file, and a CSV file `<prefix>.instruction-values.csv`.

### Human readable log file

This file contains a log of the execution of the Pin plugin.
It does not contain the resulting read/written values of the instructions.

### `<prefix>.instruction-values.csv`

This file is intended to be parsed by another application.

The `util/pretty-print-csvs.py` helper script can be used to print the contents of this CSV file in a human readable format.
This script is also used in the unit tests (`check` target).
For example:
```bash
util/pretty-print-csvs.py --prefix=path/to/prefix --log_file=path/to/instructionvalues.log
```
will print the contents of `path/to/instructionvalues.log` and `path/to/prefix.instruction-values.csv`.

This CSV file contains the following fields:
- `image_name`: the filename of the image this instruction belongs to
- `image_offset`: the offset from the start of the image of this instruction
- `num_operands`: the number of tracked operands of the instruction. This determines how many of the operand-specific fields are valid. Note that this is not necessarily the same as the number of operands indicated by `INS_OperandCount`, as `num_operands` only contains the operands whose values are tracked by the Pintool.

There are also some fields that are repeated for each operand.
In the field names below, `<n>` refers to the index of the operand, where `<n>` is in the range `[0, num_operands-1]`:
- `operand_<n>_repr`: a string representation of operand `<n>`
- `operand_<n>_width`: the width of operand `<n>` in bits
- `operand_<n>_is_read`: true if operand `<n>` was read; false otherwise
- `operand_<n>_is_written`: true if operand `<n>` was written; false otherwise
- `operand_<n>_read_values`: the set of values of operand `<n>`, sampled _before_ the instruction
- `operand_<n>_written_values`: the set of values of operand `<n>`, sampled _after_ the instruction
