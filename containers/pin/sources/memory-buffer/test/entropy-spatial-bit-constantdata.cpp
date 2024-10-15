// RUN: g++ %s -o %t.exe
// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -output %t.log -entropy_sample_interval 12 -replay -replay:basename %t/pinball -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t.log --log_file=%t.log | FileCheck %s

#include <algorithm>
#include <array>
#include <new>

int main(int argc, char *argv[]) {
  // CHECK:      {{[^[:space:]]+}}/libc.so.6::malloc(size = 4)
  // CHECK-NEXT: ---> address = 0x[[#%x,BUF1:]]
  unsigned char *buf1 = new unsigned char[4];

  // buf1 will be filled with:
  // - 2 sample intervals: 0x00 00 00 00 --> h(0/32) = 0
  // - 2 sample intervals: 0xFF FF FF FF --> h(32/32) = 0
  // - 1 sample interval:  0xF0 F0 F0 F0 --> h(16/32) = 1
  // ---> average spatial entropy = h(16/32) / 5 = 1/5.
  // ---> average bit value = 2/5 * 0 + 2/5 * 1 + 1/5 * 0.5 = 0.5

  unsigned char data[] = {0x00, 0xF0, 0xFF, 0x00, 0xFF};

  for (std::size_t sample_interval = 0; sample_interval < 5;
       ++sample_interval) {
    for (std::size_t i = 0; i < 12; ++i) {
      buf1[i % 4] = data[sample_interval];
    }
  }

  delete[] buf1;
  return 0;
}

// clang-format off

// CHECK: MEMORY BUFFERS
// CHECK: ==============

// CHECK:      Buffer:                                Buffer 0x[[#BUF1]] --> 0x[[#BUF1 + 4]]
// CHECK-NEXT: Number of reads:                       0
// CHECK-NEXT: Number of writes:                      60
// CHECK:      Average spatial entropy (bit_shannon):
// CHECK-SAME: 0.2
// CHECK:      Average spatial entropy (bit_average):
// CHECK-SAME: 0.5

// clang-format on
