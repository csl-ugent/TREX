// RUN: g++ %s -o %t.exe -masm=intel

// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -output %t.log -replay -replay:basename %t/pinball -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t.log --log_file=%t.log | \
// RUN: FileCheck %s -DEXE_PATH=%t.exe --check-prefixes CHECK,%arch,%arch-NOLIM

// RUN: %sde -log -log:basename %t.limit/pinball -- %t.exe
// RUN: %sde %toolarg -output %t.limit.log -instruction_values_limit 2 -replay -replay:basename %t.limit/pinball -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t.limit.log --log_file=%t.limit.log | \
// RUN: FileCheck %s -DEXE_PATH=%t.exe --check-prefixes CHECK,%arch,%arch-LIM

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
// CHECK:          Values (read):
// X64-NOLIM-SAME: [01 00 00 00 00 00 00 00, 02 00 00 00 00 00 00 00, 03 00 00 00 00 00 00 00]
// X64-LIM-SAME:   [01 00 00 00 00 00 00 00, 02 00 00 00 00 00 00 00]
// X86-NOLIM-SAME: [01 00 00 00, 02 00 00 00, 03 00 00 00]
// X86-LIM-SAME:   [01 00 00 00, 02 00 00 00]
// CHECK:          Values (written):
// CHECK-SAME:     []

// (2) Write instruction.

// X64:            Address: [[EXE_PATH]]::.dummy_text::my_memcopy+0x14
// X86:            Address: [[EXE_PATH]]::.dummy_text::my_memcopy+0x1c
// CHECK:          Values (read):
// CHECK-SAME:     []
// CHECK:          Values (written):
// X64-NOLIM-SAME: [01 00 00 00 00 00 00 00, 02 00 00 00 00 00 00 00, 03 00 00 00 00 00 00 00]
// X64-LIM-SAME:   [01 00 00 00 00 00 00 00, 02 00 00 00 00 00 00 00]
// X86-NOLIM-SAME: [01 00 00 00, 02 00 00 00, 03 00 00 00]
// X86-LIM-SAME:   [01 00 00 00, 02 00 00 00]
