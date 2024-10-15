// RUN: g++ %s -o %t.exe
// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -output %t.log -entropy_sample_interval 4 -replay -replay:basename %t/pinball -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t.log --log_file=%t.log | FileCheck %s

#include <cstdint>
#include <new>

int main(int argc, char *argv[]) {
  // CHECK:      {{[^[:space:]]+}}/libc.so.6::malloc(size = 8)
  // CHECK-NEXT: ---> address = 0x[[#%x,BUF1:]]
  std::uint16_t *buf = new std::uint16_t[4];

  std::uint16_t values[] = {0x1f46, 0x05de, 0x210e, 0x4106};

  // Write values
  for (int i = 0; i < 16; ++i) {
    buf[i % 4] = values[i % 4];
  }

  // Write their complement, so that every bit is 0/1 with equal probability.
  // --> average temporal entropy = 1
  for (int i = 0; i < 16; ++i) {
    buf[i % 4] = ~values[i % 4];
  }

  delete[] buf;

  return 0;
}

// CHECK: MEMORY BUFFERS
// CHECK: ==============

// CHECK:      Buffer:                   Buffer 0x[[#BUF1]] --> 0x[[#BUF1 + 8]]
// CHECK-NEXT: Number of reads:          0
// CHECK-NEXT: Number of writes:         32
// CHECK: Average temporal entropy (bit_shannon):
// CHECK-SAME: 1
