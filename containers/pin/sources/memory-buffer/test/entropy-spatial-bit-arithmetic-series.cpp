// RUN: g++ %s -o %t.exe
// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -output %t.log -entropy_sample_interval 12 -replay -replay:basename %t/pinball -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t.log --log_file=%t.log | FileCheck %s

#include <algorithm>
#include <array>
#include <new>

void fill_array(unsigned char *buf, std::size_t size) {
  // Perform 60 writes = 5 sample intervals.
  for (std::size_t i = 0; i < 60; ++i) {
    buf[i % size] = i % size;
  }
}

int main(int argc, char *argv[]) {
  // CHECK:      {{[^[:space:]]+}}/libc.so.6::malloc(size = 1)
  // CHECK-NEXT: ---> address = 0x[[#%x,BUF1:]]
  unsigned char *buf1 = new unsigned char[1];

  // CHECK:      {{[^[:space:]]+}}/libc.so.6::malloc(size = 2)
  // CHECK-NEXT: ---> address = 0x[[#%x,BUF2:]]
  unsigned char *buf2 = new unsigned char[2];

  // CHECK:      {{[^[:space:]]+}}/libc.so.6::malloc(size = 3)
  // CHECK-NEXT: ---> address = 0x[[#%x,BUF3:]]
  unsigned char *buf3 = new unsigned char[3];

  // CHECK:      {{[^[:space:]]+}}/libc.so.6::malloc(size = 4)
  // CHECK-NEXT: ---> address = 0x[[#%x,BUF4:]]
  unsigned char *buf4 = new unsigned char[4];

  // bufi will be filled with {i-1, i, ...}.
  // buf1: 0x00                --> h(0/8)
  // buf2: 0x00 0x01           --> h(1/16)
  // buf3: 0x00 0x01 0x02      --> h(2/24)
  // buf4: 0x00 0x01 0x02 0x03 --> h(4/32)
  fill_array(buf1, 1);
  fill_array(buf2, 2);
  fill_array(buf3, 3);
  fill_array(buf4, 4);

  delete[] buf1;
  delete[] buf2;
  delete[] buf3;
  delete[] buf4;

  return 0;
}

// clang-format off

// CHECK: MEMORY BUFFERS
// CHECK: ==============

// CHECK:      Buffer:                                Buffer 0x[[#BUF1]] --> 0x[[#BUF1 + 1]]
// CHECK-NEXT: Number of reads:                       0
// CHECK-NEXT: Number of writes:                      60
// CHECK:      Average spatial entropy (bit_shannon):
// CHECK-SAME: 0
// CHECK:      Average spatial entropy (bit_average):
// CHECK-SAME: 0

// CHECK:      Buffer:                                Buffer 0x[[#BUF2]] --> 0x[[#BUF2 + 2]]
// CHECK-NEXT: Number of reads:                       0
// CHECK-NEXT: Number of writes:                      60
// CHECK:      Average spatial entropy (bit_shannon):
// CHECK-SAME: 0.33729
// CHECK:      Average spatial entropy (bit_average):
// CHECK-SAME: 0.0625

// CHECK:      Buffer:                                Buffer 0x[[#BUF3]] --> 0x[[#BUF3 + 3]]
// CHECK-NEXT: Number of reads:                       0
// CHECK-NEXT: Number of writes:                      60
// CHECK:      Average spatial entropy (bit_shannon):
// CHECK-SAME: 0.41381
// CHECK:      Average spatial entropy (bit_average):
// CHECK-SAME: 0.08333

// CHECK:      Buffer:                                Buffer 0x[[#BUF4]] --> 0x[[#BUF4 + 4]]
// CHECK-NEXT: Number of reads:                       0
// CHECK-NEXT: Number of writes:                      60
// CHECK:      Average spatial entropy (bit_shannon):
// CHECK-SAME: 0.54356
// CHECK:      Average spatial entropy (bit_average):
// CHECK-SAME: 0.125

// clang-format off
