// RUN: g++ %s -o %t.exe
// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -output %t.log -replay -replay:basename %t/pinball -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t.log --log_file=%t.log | FileCheck %s

#include <new>

int main(int argc, char *argv[]) {
  // Ensure the buffers are at the same address.

  // CHECK:      {{[^[:space:]]+}}/libc.so.6::malloc(size = 1)
  // CHECK-NEXT: ---> address = 0x[[#%x,BUF:]]
  // CHECK:      {{[^[:space:]]+}}/libc.so.6::free(ptr = 0x[[#BUF]])

  // CHECK:      {{[^[:space:]]+}}/libc.so.6::malloc(size = 1)
  // CHECK-NEXT: ---> address = 0x[[#BUF]]
  // CHECK:      {{[^[:space:]]+}}/libc.so.6::free(ptr = 0x[[#BUF]])

  // CHECK:      {{[^[:space:]]+}}/libc.so.6::malloc(size = 1)
  // CHECK-NEXT: ---> address = 0x[[#BUF]]
  // CHECK:      {{[^[:space:]]+}}/libc.so.6::free(ptr = 0x[[#BUF]])

  // CHECK:      {{[^[:space:]]+}}/libc.so.6::malloc(size = 1)
  // CHECK-NEXT: ---> address = 0x[[#BUF]]
  // CHECK:      {{[^[:space:]]+}}/libc.so.6::free(ptr = 0x[[#BUF]])

  for (int i = 0; i < 4; ++i) {
    char *buf = new char[1];

    buf[0] = i + 1;

    delete[] buf;
  }

  return 0;
}

// Ensure that there are 4 buffers, with one write each, and no reads.

// CHECK: MEMORY BUFFERS
// CHECK: ==============

// CHECK:      Buffer:           Buffer 0x[[#BUF]] --> 0x[[#%x,BUF+1]]
// CHECK-NEXT: Number of reads:  0
// CHECK-NEXT: Number of writes: 1

// CHECK:      Buffer:           Buffer 0x[[#BUF]] --> 0x[[#%x,BUF+1]]
// CHECK-NEXT: Number of reads:  0
// CHECK-NEXT: Number of writes: 1

// CHECK:      Buffer:           Buffer 0x[[#BUF]] --> 0x[[#%x,BUF+1]]
// CHECK-NEXT: Number of reads:  0
// CHECK-NEXT: Number of writes: 1

// CHECK:      Buffer:           Buffer 0x[[#BUF]] --> 0x[[#%x,BUF+1]]
// CHECK-NEXT: Number of reads:  0
// CHECK-NEXT: Number of writes: 1
