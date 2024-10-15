// RUN: g++ %s -o %t.exe -masm=intel

// RUN: echo '1 3 2 4 6 5 8' | \
// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -output %t.log -replay -replay:basename %t/pinball -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t.log --log_file=%t.log | \
// RUN: FileCheck %s -DEXE_PATH=%t.exe

// XFAIL: x86

#include <cstdint>
#include <iostream>

// Returns 0 or 1 depending on the parity of 'n'.
extern "C" std::int64_t is_odd(std::int64_t n);

asm(R"(
    .section        .text
    .intel_syntax   noprefix
    .globl          is_odd
    .type           is_odd, @function

is_odd:
    and             rdi, 1
    cmp             rdi, 0
    jne             .odd

.even:
    mov             rax, 0
    jmp             .end

.odd:
    mov             rax, 1
    jmp             .end

.end:
    ret
)");

int main(int argc, char *argv[]) {
  // Read the inputs from stdin.
  std::int64_t i;

  while (std::cin >> i) {
    std::cout << is_odd(i) << std::endl;
  }

  return 0;
}

// clang-format off

// CHECK: BASIC BLOCKS
// CHECK: ============

// entry basic block
// =================

// CHECK: Address range: [[EXE_PATH]]::.dummy_text::is_odd+0x0 --> [[EXE_PATH]]::.dummy_text::is_odd+0xa
// CHECK: Number of executions:
// CHECK-SAME: 7

// .even basic block
// =================

// CHECK: Address range: [[EXE_PATH]]::.dummy_text::is_odd+0xa --> [[EXE_PATH]]::.dummy_text::is_odd+0x13
// CHECK: Number of executions:
// CHECK-SAME: 4

// .odd basic block
// ================

// CHECK: Address range: [[EXE_PATH]]::.dummy_text::is_odd+0x13 --> [[EXE_PATH]]::.dummy_text::is_odd+0x1c
// CHECK: Number of executions:
// CHECK-SAME: 3

// .end basic block
// ================

// CHECK: Address range: [[EXE_PATH]]::.dummy_text::is_odd+0x1c --> [[EXE_PATH]]::.dummy_text::is_odd+0x1d
// CHECK: Number of executions:
// CHECK-SAME: 7
