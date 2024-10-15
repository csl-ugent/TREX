// RUN: gcc %s -o %t.exe

// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -csv_prefix %t -replay -replay:basename %t/pinball \
// RUN: -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t | \
// RUN: FileCheck %s

#include <stdio.h>
#include <sys/mman.h>

int main(int argc, char **argv) {
  // CHECK-DAG: mmap{{2?}}
  // CHECK-DAG: mprotect
  // CHECK-DAG: munmap

  // CHECK: mmap{{2?}}(0x0, 12345, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x[[#%x,ADDR:]]
  void *addr = mmap(NULL, 12345, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  // CHECK: munmap(0x[[#ADDR]], 12345) = 0
  munmap(addr, 12345);

  printf("Hello, world!\n");

  return 0;
}
