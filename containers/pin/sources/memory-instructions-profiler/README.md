# Memory Instructions Profiler Pin Plugin

## Description

This Pin tool profiles memory instructions, i.e. instructions that read from or write to memory.
For each memory instruction, it collects:
- A limited set of values that are read or written by this instruction.
- The byte Shannon entropy of all the values that this instruction has read or written over the course of all its executions.
- Some statistics about the instruction:
    - The total number of bytes read/written over all executions
    - The number of times this instruction was executed
    - The number of unique byte addresses that this instruction read from or wrote to

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

Use `pin -t libMemoryInstructionsProfiler.so -help -- ls` for a list of all available options.
Options that are specific to this Pin tool:

```
-csv_prefix  [default ]
	Set the prefix used for the CSV output file. The output file will be
	of the form <prefix>.memory-instructions.csv.
-end_after_main  [default 1]
	When true, ends analysis after main() is finished
-instruction_values_limit  [default 5]
	Number of unique read/written values to keep per static instruction.
-o  [default memoryinstructionsprofiler.log]
	Specify the filename of the human-readable log file
-start_from_main  [default 1]
	When true, starts analysis from the point that main() is called
```

## Output

The Pin tool outputs two files: a human readable log file, and a CSV file `<prefix>.memory-instructions.csv`.

### Human readable log file

This file contains a log of the execution of the Pin plugin.
It does not contain the resulting memory instructions that are found.

### `<prefix>.memory-instructions.csv`

This file is intended to be parsed by another application.

The `util/pretty-print-csvs.py` helper script can be used to print the contents of this CSV file in a human readable format.
This script is also used in the unit tests (`check` target).
For example:
```bash
util/pretty-print-csvs.py --prefix=path/to/prefix --log_file=path/to/memoryinstructionsprofiler.log
```
will print the contents of `path/to/memoryinstructionsprofiler.log` and `path/to/prefix.memory-instructions.csv`.

This CSV file contains the following fields:
- `ip`: the address of the instruction (i.e. the value of the instruction
  pointer)
- `image_name`: the filename of the image this instruction belongs to
- `full_image_name`: the full path of the image this instruction belongs to
- `section_name`: the name of the section this instruction belongs to
- `image_offset`: the offset from the start of the image of this instruction
- `routine_name`: the name of the instruction this instruction belongs to
- `routine_offset`: the offset from the start of the routine of this
  instruction
- `read_values` and `written_values`: a comma separated list of values
  read/written by this instruction. Bytes are separated by spaces. Values that
  are read/written in different executions are separated with a comma. Bytes
  are ordered as they occur in memory (i.e. little endian on Intel). Examples
  include `[]` (instruction does not write any values), `[ab cd]` (instruction
  writes the two-byte value `0xcdab` to memory) and `[ab cd, 01 02]`
  (instruction writes the two-byte value `0xcdab` and the two-byte value
  `0x0201` to memory).
- `read_values_entropy` and `written_values_entropy`: the byte Shannon entropy
  of all the values that this instruction has read or written over the course
  of all its executions.
- `num_bytes_read` and `num_bytes_written`: the total number of bytes that this
  instruction has read or written over the course of all its executions.
- `num_unique_byte_addresses_read` and `num_unique_byte_addresses_written`: the
  total number of unique byte addresses that this instruction reads from or
  writes to. Note that a 2-byte write represents two addresses!
- `num_executions`: The number of times this instruction was executed.
