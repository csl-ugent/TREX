// RUN: g++ %s -o %t.exe -masm=intel

// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -output %t.log -replay -replay:basename %t/pinball -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t.log --log_file=%t.log | \
// RUN: FileCheck %s -DEXE_PATH=%t.exe --check-prefixes CHECK,%arch

#include "common/memcopy.h"

int main() {
  native_int from[4] = {1, 2, 3, 1};
  native_int to[4] = {};

  my_memcopy(to, from, 4);

  return 0;
}

// clang-format off

// CHECK: MEMORY INSTRUCTIONS
// CHECK: ===================

// (1) Read instruction.

// X64:            Address: [[EXE_PATH]]::.dummy_text::my_memcopy+0x10
// X86:            Address: [[EXE_PATH]]::.dummy_text::my_memcopy+0x14
// CHECK:          Entropy of values (read):
// X64-SAME:       0.091383
// X86-SAME:       0.148285
// CHECK:          Entropy of values (written):
// CHECK-SAME:     0.000000

// (2) Write instruction.

// X64:            Address: [[EXE_PATH]]::.dummy_text::my_memcopy+0x14
// X86:            Address: [[EXE_PATH]]::.dummy_text::my_memcopy+0x1c
// CHECK:          Entropy of values (read):
// CHECK-SAME:     0.000000
// CHECK:          Entropy of values (written):
// X64-SAME:       0.091383
// X86-SAME:       0.148285
