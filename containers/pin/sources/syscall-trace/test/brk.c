// RUN: gcc %s -o %t.exe

// RUN: %sde -log -log:basename %t/pinball -- %t.exe >%t.appout
// RUN: %sde %toolarg -csv_prefix %t -replay -replay:basename %t/pinball \
// RUN: -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t | \
// RUN: FileCheck %s

#include <stdio.h>
#include <stdlib.h>

int main() {
  // CHECK:      brk(0x0) = 0x[[#%x,BRK_INIT:]]
  // CHECK:      brk(0x0) = 0x[[#BRK_INIT]]
  // CHECK:      brk(0x[[#%x,NEW_BREAK:]])
  // CHECK-SAME: = 0x[[#NEW_BREAK]]

  void *heap = malloc(4);
  printf("heap address = %p\n", heap);

  return 0;
}
