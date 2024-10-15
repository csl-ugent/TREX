# Memory Buffer Pin Plugin

## Description

The purpose of this Pin tool is threefold:

1. It finds memory buffers (malloc'ed memory) and memory regions (aligned and adjacent memory locations), their address range, the number of reads from/writes to them, and their spatial and temporal entropy.
2. For instructions that read from/write to memory, it traces their address, opcode, operands, and the values read or written by them.
3. It links memory buffers and regions to the instructions that read from or write to them.

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

Use `pin -t libTool.so -help -- ls` for a list of all available options.
Options that are specific to this Pin tool:

```
-csv_prefix  [default ]
	Set the prefix used for the CSV output files. The output files will be
	of the form <prefix>.instructions.csv etc.
-end_after_main  [default 1]
	When true, ends analysis after main() is finished
-entropy_sample_interval  [default 100]
	Sample interval in time for the calculations of spatial and temporal
	entropy. This setting determines how frequently spatial and temporal
	entropy is sampled for a given memory buffer or region. It is
	expressed in number of writes to that specific memory buffer or
	region.
-instruction_values_limit  [default 5]
	Number of unique read/written values to keep per static instruction.
-o  [default tool.log]
	Specify the filename of the human-readable log file
-start_from_main  [default 1]
	When true, starts analysis from the point that main() is called
```

## Output

The Pin tool outputs four files: a human readable log file, and three CSV files.

### Human readable log file

This file contains a log of the execution of the Pin plugin.
It does not contain the end result, such as the memory buffers that are found.

### CSV files

The CSV files are intended to be parsed by another application.

The `util/pretty-print-csvs.py` helper script can be used to print the contents of the CSV files in a human readable format.
This script is used in the unit tests (`check` target).
For example:
```bash
util/pretty-print-csvs.py --prefix=path/to/prefix --log_file=path/to/tool.log
```
will print the contents of `path/to/tool.log`, `path/to/prefix.buffers.csv`, `path/to/prefix.regions.csv`, and `path/to/prefix.instructions.csv`.

#### `<prefix>.buffers.csv` and `<prefix>.regions.csv`

These files contain information about memory buffers and memory regions, respectively.
The fields are:

- `id`: counter that is used as a unique identifier within a file
- `start_address` and `end_address`: determine the range of the memory
  buffer/region
- `num_reads` and `num_writes`: the number of reads from/writes to the memory
  buffer/region
- `average_spatial_entropy_<metric>` and `average_temporal_entropy_<metric>`:
  the spatial/temporal entropy of the memory region/buffer calculated using
  `<metric>`, averaged over the entire execution. For the possible values of
  `<metric>`, refer to [Entropy metrics](#entropy-metrics).
- `read_ips` and `write_ips`: the addresses of the instructions that
  respectively read from or write to that memory buffer/region
- `allocation_address`: the address of the instruction that 'allocated' this buffer. This is determined by calling `backtrace` in `malloc`, and returning the first instruction not in libc.
- `DEBUG_allocation_backtrace`: backtrace of the allocation of a memory buffer.
  Empty string for memory regions. Note that this field should only be used for
  debugging purposes. Newlines are replaced by `|`.
- `DEBUG_annotation`: user-provided information for this memory buffer.
  Note that this field should only be used for debugging purposes. A memory buffer
  can be annotated by calling
  `MATE_FRAMEWORK_DEBUG_ANNOTATE_MEMORY(const char* address, unsigned int size, const char* annotation, const char* filename, unsigned int line)`.

#### `<prefix>.instructions.csv`

This file contains information about instructions.
The fields are:

- `ip`: the address of the instruction (i.e. the value of the instruction
  pointer)
- `image_name`: the filename of the image this instruction belongs to
- `full_image_name`: the full path of the image this instruction belongs to
- `section_name`: the name of the section this instruction belongs to
- `image_offset`: the offset from the start of the image of this instruction
- `routine_name`: the name of the instruction this instruction belongs to
- `routine_offset`: the offset from the start of the routine of this
  instruction
- `opcode`: the opcode of the instruction
- `operands`: a string representing the operands of the instruction
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
  of all its executions
- `num_bytes_read` and `num_bytes_written`: the total number of bytes that this
  instruction has read or written over the course of all its executions

## Calculation of the entropy

### General explanation

You can think of the contents of a memory buffer or region as a large matrix,
where each row represents the content of the entire buffer or region at a
particular point in time, and each column represents a fixed address.

A new row is added to this matrix (i.e. the contents of the memory buffer are
sampled) every `x` writes to that memory buffer or region. The value of `x`
represents the sample interval in the time dimension, and can be changed using
the `-entropy_sample_interval` command-line option.

Entropy metrics can be calculated at the bit or byte level, depending on
whether each element in this memory matrix represents a byte or a bit.

Each entropy metric may be calculated spatially or temporally. Spatial
entropies are calculated for every snapshot of the buffer at a point in time (a
row), and then averaged over all snapshots (all rows). Temporal entropies are
calculated for every address of the buffer (a column), and then averaged over
all bits or bytes in the buffer (all columns).

### Entropy metrics

This section explains the meaning of the different metrics for spatial and
temporal entropy.

#### Bit-level Shannon entropy (`bit_shannon`)

This metric corresponds to Shannon's definition of entropy at the bit level.
The resulting entropy is `-p_0 log2(p_0) - p_1 log2(p_1)`, where `p_0` is the
fraction of 0-bits, and `p_1` is the fraction of 1-bits in a bitstring.

#### Byte-level Shannon entropy (`byte_shannon`)

This metric corresponds to Shannon's definition of entropy at the byte level.
The resulting entropy is `sum(-p_i * log2(p_i), i = 0..255) / log2(256)`, where
`p_i` is the fraction of bytes with the value `i` in a byte string.

#### Adapted byte-level Shannon entropy (`byte_shannon_adapted`)

This is analogous to the byte-level Shannon entropy, with the only difference
that the scaling factor is changed: `sum(-p_i * log2(p_i), i = 0..255) /
log2(min(N, 256))`. This is to provide more accurate entropy values for buffers
smaller than 256 bytes.

#### Number of different bytes (`byte_num_different`)

This metric counts the number of bytes in a byte string, counting repeated
bytes as only occurring once.

#### Number of unique bytes (`byte_num_unique`)

This metric counts the number of bytes that only occur once in a byte string.

#### Average bit-value (`bit_average`)

This metric calculates the average value at the bit level.

#### Average byte-value (`byte_average`)

This metric calculates the average value at the byte level.

### Combinations

Every metric for entropy can be applied either spatially or temporally. The
following table lists the supported combinations. Refer to [Entropy
metrics](#entropy-metrics) for more information on each of these metrics.

| Metric                             | Spatial            | Temporal           |
|------------------------------------|--------------------|--------------------|
| Bit-level Shannon entropy          | :heavy_check_mark: | :heavy_check_mark: |
| Byte-level Shannon entropy         | :heavy_check_mark: | :x:                |
| Adapted byte-level Shannon entropy | :heavy_check_mark: | :x:                |
| Number of different bytes          | :heavy_check_mark: | :x:                |
| Number of unique bytes             | :heavy_check_mark: | :x:                |
| Average bit-value                  | :heavy_check_mark: | :white_check_mark: |
| Average byte-value                 | :heavy_check_mark: | :white_check_mark: |

**Legend:**

- :heavy_check_mark: - Implemented
- :white_check_mark: - Can be achieved using other metric
- :running:          - TODO, planned to be implemented
- :x:                - Unsupported

Note that the average value per bit/byte is the same for spatial and temporal
entropy. As such, only the spatial variant is implemented.
