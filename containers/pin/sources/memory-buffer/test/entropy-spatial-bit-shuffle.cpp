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

  // buf1 will be filled with the bytes 0xec (hamming weight 5), 0x20 (hamming
  // weight 1), 0x8b (hamming weight 4), 0x5b (hamming weight 5) in some order.
  // This means the spatial entropy of buf1 will be h(15/32) at every sample.
  // The average bit value is 15/32 at every sample.
  unsigned char data[] = {0xec, 0x20, 0x8b, 0x5b};

  // Do this for 60 writes = 5 sample intervals, shuffling after every 4 writes.
  for (std::size_t i = 0; i < 60; ++i) {
    if (i % 4 == 0)
      std::random_shuffle(std::begin(data), std::end(data));
    buf1[i % 4] = data[i % 4];
  }

  delete[] buf1;

  return 0;
}

// clang-format off

// CHECK:      Buffer:                                Buffer 0x[[#BUF1]] --> 0x[[#BUF1 + 4]]
// CHECK-NEXT: Number of reads:                       0
// CHECK-NEXT: Number of writes:                      60
// CHECK:      Average spatial entropy (bit_shannon):
// CHECK-SAME: 0.99718
// CHECK:      Average spatial entropy (bit_average):
// CHECK-SAME: 0.46875

// clang-format on
