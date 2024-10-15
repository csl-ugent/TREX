// RUN: gcc %s -o %t.exe
// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -output %t.log -replay -replay:basename %t/pinball -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix %t.log --log_file=%t.log | FileCheck %s

#include <stdlib.h>

int main(int argc, char *argv[]) {
  // CHECK:      {{[^[:space:]]+}}/libc.so.6::malloc(size = 1)
  char *mem1 = malloc(1 * sizeof(char));

  // CHECK:      {{[^[:space:]]+}}/libc.so.6::malloc(size = 2)
  // CHECK-NEXT: ---> address = 0x[[#%x,BUF2:]]
  char *mem2 = malloc(2 * sizeof(char));

  // CHECK:      {{[^[:space:]]+}}/libc.so.6::malloc(size = 3)
  // CHECK-NEXT: ---> address = 0x[[#%x,BUF3:]]
  char *mem3 = malloc(3 * sizeof(char));

  // CHECK:      {{[^[:space:]]+}}/libc.so.6::malloc(size = 4)
  // CHECK-NEXT: ---> address = 0x[[#%x,BUF4:]]
  char *mem4 = malloc(4 * sizeof(char));

  // CHECK:      {{[^[:space:]]+}}/libc.so.6::malloc(size = 5)
  // CHECK-NEXT: ---> address = 0x[[#%x,BUF5:]]
  char *mem5 = malloc(5 * sizeof(char));

  for (int i = 0; i < 5; ++i)
    mem5[i] = i;

  for (int i = 0; i < 1; ++i)
    mem1[i] = mem5[i];

  for (int i = 0; i < 2; ++i)
    mem2[i] = mem5[i];

  for (int i = 0; i < 3; ++i)
    mem3[i] = mem5[i];

  for (int i = 0; i < 4; ++i)
    mem4[i] = mem5[i];

  // CHECK: {{[^[:space:]]+}}/libc.so.6::free(ptr = 0x[[#%x,]])
  free(mem1);

  // CHECK: {{[^[:space:]]+}}/libc.so.6::free(ptr = 0x[[#BUF2]])
  free(mem2);

  // CHECK: {{[^[:space:]]+}}/libc.so.6::free(ptr = 0x[[#BUF3]])
  free(mem3);

  // CHECK: {{[^[:space:]]+}}/libc.so.6::free(ptr = 0x[[#BUF4]])
  free(mem4);

  // CHECK: {{[^[:space:]]+}}/libc.so.6::free(ptr = 0x[[#BUF5]])
  free(mem5);

  return 0;
}

// CHECK: MEMORY BUFFERS
// CHECK: ==============

// CHECK:      Buffer:           Buffer 0x[[#BUF2]] --> 0x[[#BUF2+2]]
// CHECK-NEXT: Number of reads:  0
// CHECK-NEXT: Number of writes: 2

// CHECK:      Buffer:           Buffer 0x[[#BUF3]] --> 0x[[#BUF3+3]]
// CHECK-NEXT: Number of reads:  0
// CHECK-NEXT: Number of writes: 3

// CHECK:      Buffer:           Buffer 0x[[#BUF4]] --> 0x[[#BUF4+4]]
// CHECK-NEXT: Number of reads:  0
// CHECK-NEXT: Number of writes: 4

// CHECK:      Buffer:           Buffer 0x[[#BUF5]] --> 0x[[#BUF5+5]]
// CHECK-NEXT: Number of reads:  10
// CHECK-NEXT: Number of writes: 5
