// RUN: g++ %s -o %t.exe
// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -output %t.log -entropy_sample_interval 1 -replay -replay:basename %t/pinball -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t.log --log_file=%t.log | FileCheck %s

#include <new>

int main(int argc, char *argv[]) {
  // CHECK:      {{[^[:space:]]+}}/libc.so.6::malloc(size = 1)
  // CHECK-NEXT: ---> address = 0x[[#%x,BUF:]]
  char *buf = new char[1];

  buf[0] = 0xF0; // 1111 0000, i.e. maximal entropy

  for (int i = 0; i < 999; ++i) {
    buf[0] ^= 0xFF; // Flip all bits: this will still read/write a value with 4
                    // 0 bits and 4 1 bits.
  }

  return 0;
}

// clang-format off

// CHECK: MEMORY BUFFERS
// CHECK: ==============

// CHECK:      Buffer:           Buffer 0x[[#BUF]] --> 0x[[#BUF+1]]
// CHECK-NEXT: Number of reads:  999
// CHECK-NEXT: Number of writes: 1000
// CHECK:      Average spatial entropy (bit_shannon):
// CHECK-SAME: 1
// CHECK:      Average spatial entropy (bit_average):
// CHECK-SAME: 0.5
// CHECK:      Average temporal entropy (bit_shannon):
// CHECK-SAME: 1

// clang-format on
