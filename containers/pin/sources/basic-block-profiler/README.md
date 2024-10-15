# Basic Block Profiler Pin Plugin

## Description

This Pin tool profiles basic blocks: for each basic block, it returns how many times it was executed.

Note that Pin discovers the control flow of a program dynamically as it is executing.
This means that Pin's definition of a basic block may be different from the classical definition.
This may result in basic blocks that overlap.

Consider this example from the Pin documentation:
```cpp
siwtch (i)
{
    case 4: total++;
    case 3: total++;
    case 2: total++;
    case 1: total++;
    case 0:
    default: break;
}
```

This might correspond to the following instruction sequence on x86:
```asm
.L7:
        addl    $1, -4(%ebp)
.L6:
        addl    $1, -4(%ebp)
.L5:
        addl    $1, -4(%ebp)
.L4:
        addl    $1, -4(%ebp)
```

When the first case is entered (`.L7`), Pin will create a basic block consisting of all four `addl` instructions.
Similarly, if the second case is entered (`.L6`), the generated basic block will consist of three `addl` instructions, and so on.

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

Use `pin -t libBasicBlockProfiler.so -help -- ls` for a list of all available options.
Options that are specific to this Pin tool:

```
-csv_prefix  [default ]
	Set the prefix used for the CSV output file. The output file will be
	of the form <prefix>.basic-blocks.csv.
-end_after_main  [default 1]
	When true, ends analysis after main() is finished
-o  [default basicblockprofiler.log]
	Specify the filename of the human-readable log file
-start_from_main  [default 1]
	When true, starts analysis from the point that main() is called
```

## Output

The Pin tool outputs two files: a human readable log file, and a CSV file `<prefix>.basic-blocks.csv`.

### Human readable log file

This file contains a log of the execution of the Pin plugin.
It does not contain the resulting basic blocks that are found.

### `<prefix>.basic-blocks.csv`

This file is intended to be parsed by another application.

The `util/pretty-print-csvs.py` helper script can be used to print the contents of this CSV file in a human readable format.
This script is also used in the unit tests (`check` target).
For example:
```bash
util/pretty-print-csvs.py --prefix=path/to/prefix --log_file=path/to/basicblockprofiler.log
```
will print the contents of `path/to/basicblockprofiler.log` and `path/to/prefix.basic-blocks.csv`.

This CSV file contains the following fields:
- `ip_begin`: the address of the first instruction in the basic block (i.e. the value of the corresponding instruction pointer)
- `ip_end`: the address of the one-past-the-last instruction in the basic block (i.e. the value of the corresponding instruction pointer)
- `image_name`: the filename of the image this basic block belongs to
- `full_image_name`: the full path of the image this basic block belongs to
- `section_name`: the name of the section this basic block belongs to
- `image_offset_begin`: the offset from the start of the image of the first instruction in the basic block
- `image_offset_end`: the offset from the start of the image of the one-past-the-last instruction in the basic block
- `routine_name`: the name of the routine this basic block belongs to
- `routine_offset_begin`: the offset from the start of the routine of the first instruction in the basic block
- `routine_offset_end`: the offset from the start of the routine of the one-past-the-last instruction in the basic block
- `num_executions`: the number of times this basic block was executed
