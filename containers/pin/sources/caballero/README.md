# Caballero Pin Plugin

## Description

This Pin plugin annotates crypto-related basic blocks based on the heuristic from the paper "Dispatcher: Enabling Active Botnet Infiltration using Automatic Protocol Reverse-Engineering" by Caballero et al.
This heuristic looks for basic blocks which have a large amount of arithmetic and bitwise operations.

The Pin plugin works as follows:

1. Instructions are either classified as Caballero (logical and shift instructions) or not Caballero (the other instructions).
2. Keep a list of the basic blocks that were executed last per thread. We only keep enough basic blocks to cover at most `MAX_WINDOW_SIZE` (default `100`) instructions.
3. At any point in time, we iteratively look at the last `n` executed basic blocks, for `n = 1, 2, ..., N`. If these `n` basic blocks contain at least `MIN_INTERVAL_SIZE` (default `15`) instructions, of which a fraction of at least `golden_ratio` (default `0.4`, i.e. `40%`) Caballero instructions, we mark the oldest basic block (i.e. the one which was executed first) as possibly crypto related.

In other words, we find all basic blocks that *start* a golden interval: a sequence of basic blocks containing between `MIN_INTERVAL_SIZE` and `MAX_WINDOW_SIZE` instructions, of which a fraction `golden_ratio` are logical and shift instructions.

## Compilation

```bash
mkdir build && cd build
cmake -DPin_ROOT_DIR=~/bin/pin-3.16-98275-ge0db48c31-gcc-linux/ ..
cmake --build .
```

**NOTE:** Make sure you use `g++` and not `clang++`!

You can also specify `-DPin_TARGET_ARCH=x86` to compile a 32-bit version of the plugin.

## Options

Use `pin -t libCaballeroPinTool.so -help -- ls` for a list of all available options.
Options that are specific to this Pin tool:

```
-csv_prefix  [default ]
	Set the prefix used for the CSV output file. The output file will be
	of the form <prefix>.caballero.csv.
-output  [default caballero.log]
	Specify the filename of the human-readable log file
-r  [default 0.4]
	specify golden ratio
```

## Output

This Pin tool outputs a CSV file `<csv_prefix>.caballero.csv`, containing all basic blocks annotated according to the Caballero heuristic.
Each line represents a basic block and contains the following fields:

- `image_name`: The image of the basic block.
- `image_offset_begin`: The offset from the beginning of the image of the start of the basic block.
- `image_offset_end`: The offset from the beginning of the image of the end of the basic block.
