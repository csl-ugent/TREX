// RUN: g++ %s -o %t.exe -masm=intel

// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -output %t.log -replay -replay:basename %t/pinball -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t.log --log_file=%t.log | \
// RUN: FileCheck %s -DEXE_PATH=%t.exe --check-prefixes CHECK,%arch

#include "common/memcopy.h"

int main() {
  native_int from[4] = {};
  native_int to[4] = {};

  for (int i = 0; i < 10; ++i) {
    my_memcopy(to, from, 4);
  }

  return 0;
}

// clang-format off

// CHECK: MEMORY INSTRUCTIONS
// CHECK: ===================

// (1) Read instruction.

// X64:            Address: [[EXE_PATH]]::.dummy_text::my_memcopy+0x10
// X86:            Address: [[EXE_PATH]]::.dummy_text::my_memcopy+0x14
// CHECK:          Number of executions:
// CHECK-SAME:     40{{$}}

// (2) Write instruction.

// X64:            Address: [[EXE_PATH]]::.dummy_text::my_memcopy+0x14
// X86:            Address: [[EXE_PATH]]::.dummy_text::my_memcopy+0x1c
// CHECK:          Number of executions:
// CHECK-SAME:     40{{$}}
