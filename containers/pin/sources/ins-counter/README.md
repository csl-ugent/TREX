# Instruction Counter Pin Plugin

Allows to count the number of executed instructions for a program, divided between the main program and used libraries.

By default it will only count everything from the start of main. Pass `-e 0` to the program to count every instruction,
including preloading calls and others.

## Compilation of Plugin

```bash
mkdir build && cd build
cmake -DPin_ROOT_DIR=~/bin/pin-3.16-98275-ge0db48c31-gcc-linux/ ..
cmake --build .
```

**NOTE:** Make sure you use `g++` and not `clang++`!

You can also specify `-DPin_TARGET_ARCH=x86` to compile a 32-bit version of the plugin.
