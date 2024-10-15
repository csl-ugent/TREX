# Data Dependencies Pin Plugin

## Description

This Pin tool collects true data dependencies (RAW): for each instruction, it records which instructions it depends on.
Both dependencies through registers and through memory are collected.

By specifying the command-line option `-shortcuts` (default `off`), the plugin logs 'shortcut' dependencies instead of real dependencies.
See [Shortcut dependencies](#shortcut-dependencies) for more information.

**NOTE:** This tool also includes dependencies through `%rip`, which may result in unintuitive dependencies (e.g. consecutive branches depending on each other), so you probably want to filter these out.

### Shortcut dependencies

#### Explanation

The only difference between shortcut dependencies and normal dependencies is how move-like instructions such as `mov`, `movq`, `movs`, ... are handled.
In essence, shortcut dependencies "look through" moves.
Thus, an instruction that uses a value copied by a move no longer depends on the move itself, but rather on the instruction that last wrote to the move's source.
You can think of a shortcut dependency of an instruction `x` on an instruction `y` as meaning: instruction `x` uses a value that was last produced by instruction `y`.

Let us give an example to make this more clear.
Suppose we have the following assembly snippet:
```asm
mov     rax, 1      # instruction 1
mov     rcx, rax    # instruction 2
mov     rdx, rcx    # instruction 3
```

With normal dependencies, instruction 3 would depend on instruction 2 through `rcx`.
But this instruction 2 is just a simple copy operation.
With shortcut dependencies, this dependency would be replaced by a dependency of instruction 3 on instruction 1, i.e. the instruction that produces the value of `rax`, which was later copied to `rcx`.

#### Extending support for move-like instructions

At the moment, each move-like instruction needs to be handled on a case-by-case basis.
This is done through the `movOperandMap` variable in `src/ddl.cpp`.

This `std::map` lists, for each mov-like opcode, which operands of an instruction are regarded as the destination and source.
For example, for a regular `mov dst, src` (`XED_ICLASS_MOV`), the destination is the first operand, and the source is the second operand, leading to `(0, 1)`.

The process to add an instruction is as follows:
1. Find the instruction opcode type corresponding to the instruction. See the file `xed-iclass-enum.h` in the Pin toolkit.
2. Add an empty entry to the `movOperandMap` in `src/ddl.cpp` for that instruction type.
3. Add the instruction to `test/shortcuts/instructions.cpp`. (Both for x64 and x86, if applicable!)
4. Run the tests using `ninja check` and find the operand indices corresponding to the destination and source in the file `build-$arch/test/shortcuts/Output/instructions.cpp.tmp.apperr`.
5. Fill the entry in `movOperandMap` with the correct indices.
6. Finally, add the necessary `CHECK` lines to `test/shortcuts/instructions.cpp`, and verify the tests pass for both x86 and x64.

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

Use `pin -t libDataDependenciesPinTool.so -help -- ls` for a list of all available options.
Options that are specific to this Pin tool:

```
-csv_prefix  [default ]
	Set the prefix used for the CSV output file. The output file will be
	of the form <prefix>.memory_dependencies.csv and
	<prefix>.register_dependencies.csv.
-end_after_main  [default 1]
	When true, ends analysis after main() is finished
-ignore_nops  [default 1]
	When true, ignore NOP instructions when constructing data
	dependencies. This also handles 'endbr' instructions, since these are
	regarded by Pin as NOPs.
-shortcuts  [default 0]
	When true, output shortcut dependencies instead of normal
	dependencies.
-start_from_main  [default 1]
	When true, starts analysis from the point that main() is called
-syscall_file  [default ]
    When set, read system call specifications from the specified file, and also propagate dependencies for them.
-v  [default 0]
	When true, print more verbose output. This is useful for debugging and
	unit tests.
```

For the `-syscall_file` argument, you need to specify the path to a file where each line represents a system call you want to track dependencies for, in the following format:

```
<syscall name>,<syscall occurrence to track, 0-based>,<number of bytes to mark as tainted>
```

## Output

The Pin tool outputs two CSV files containing the found dependencies: `<prefix>.memory_dependencies.csv` and `<prefix>.register_dependencies.csv`, and one CSV file which contains a list of instructions related to system calls: `<prefix>.syscall_instructions.csv`.

### CSV files

The CSV files are intended to be parsed by another application.

The `util/pretty-print-csvs.py` helper script can be used to print the contents of the CSV files in a human readable format.
This script is used in the unit tests (`check` target).
For example:
```bash
util/pretty-print-csvs.py --prefix=path/to/prefix
```
will print the contents of `path/to/prefix.memory_dependencies.csv`, `path/to/prefix.register_dependencies.csv`, and `path/to/prefix.syscall_instructions.csv`.

#### `<prefix>.memory_dependencies.csv`

This file contains information about memory dependencies.
This CSV file contains fields related to the "write" instruction, and the "read" instruction.
Each entry contains one true dependency: the read instruction depends on the write instruction.

- `Write_img`: The filename of the image of the write instruction.
- `Write_off`: The offset from the start of the image of the write instruction.
- `Memory`: The memory address corresponding to this dependency.
- `Read_img`: The filename of the image of the read instruction.
- `Read_off`: The offset from the start of the image of the read instruction.

#### `<prefix>.register_dependencies.csv`

This file contains information about register dependencies.
This CSV file contains fields related to the "write" instruction, and the "read" instruction.
Each entry contains one true dependency: the read instruction depends on the write instruction.

- `Write_img`: The filename of the image of the write instruction.
- `Write_off`: The offset from the start of the image of the write instruction.
- `Register`: The register corresponding to this dependency.
- `Read_img`: The filename of the image of the read instruction.
- `Read_off`: The offset from the start of the image of the read instruction.

#### `<prefix>.syscall_instructions.csv`

This file contains information about instructions that invoke one of the system calls specified via the `syscall_file` argument.
Each entry corresponds to one such instruction, and has the following fields:

- `image_name`: The filename of the image of the instruction.
- `image_offset`: The offset from the start of the image of the instruction.
- `index`: The index of the system call in the `syscall_file`.
- `thread_id`: The ID of the thread that invoked the system call.
- `syscall_id`: The ID of the system call within its thread.
