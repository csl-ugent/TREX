# Binary Ninja integration

## Installation instructions (for GNU/Linux)

To install the plugin:

```bash
mkdir -p ~/.binaryninja/plugins
cd /path/to/mate-attack-framework/integration/binaryninja/
ln -s $(pwd) ~/.binaryninja/plugins/mate_attack_framework
```

To install the BinaryNinja API for headless operation, run the following in the virtual environment of the framework:

```bash
python3 /path/to/binaryninja/scripts/install_api.py
```

## Features

### Showing dataflow graph

The data dependencies between different instructions can be visualised using the DFG:

- Show DFG for the current function (`Tools > MATE Attack Framework > Dependencies > Show data flow graph for function`)
- Show DFG for the currently selected instruction (`Tools > MATE Attack Framework > Dependencies > Show data flow graph for instruction`)

### Highlight dependencies

The data dependencies can also be visualised in the CFG itself.
By highlighting an instruction `I`, all instructions that `I` depends on in the current function will be highlighted.

You can enable/disable this highlight in the "Instruction info" dock under `Data dependencies > Highlight dependencies`.

### VSA for memory reads

The Pin tool records for each memory read which values are read.
Using `MATE Attack Framework > Memory buffers > Apply VSA to loads`, each load is annotated with these values using Binary Ninja's built-in value set analysis (VSA).

### Instruction info dock

This dock can be shown/hidden using `View > Other Docks > Show/Hide MATE Attack Framework: Instruction info`.

It lists the read/written values for the currently selected instruction, and allows enabling/disabling the [dependencies highlight](#highlight-dependencies).

### Memory buffer info dock

This dock can be shown/hidden using `View > Other Docks > Show/Hide MATE Attack Framework: Memory buffer info`.

By default, it lists the memory buffers (i.e. regions of memory obtained via `malloc` or `new`) and regions (i.e. aligned regions of a certain size) that the currently selected instruction reads from or writes to.
If the checkbox `Limit to current instruction only` is not checked, all memory buffers and regions are shown.

When selecting a memory buffer or region from the `Memory buffer list`, the `Memory buffer references` view will show the instructions that read from/write to this currently selected memory buffer or region.
Double clicking an instruction will navigate to it.
