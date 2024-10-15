// RUN: g++ %s -o %t.exe -masm=intel

// RUN: echo '1 3 2 4 6 5 8' | \
// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -output %t.log -replay -replay:basename %t/pinball -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t.log --log_file=%t.log | \
// RUN: FileCheck %s -DEXE_PATH=%t.exe

// XFAIL: x64

#include <cstdint>
#include <iostream>

// Returns 0 or 1 depending on the parity of 'n'.
extern "C" std::int32_t is_odd(std::int32_t n);

asm(R"(
    .section        .text
    .intel_syntax   noprefix
    .globl          is_odd
    .type           is_odd, @function

is_odd:
    mov             eax, DWORD PTR 4[esp]
    and             eax, 1
    cmp             eax, 0
    jne             .odd

.even:
    mov             eax, 0
    jmp             .end

.odd:
    mov             eax, 1
    jmp             .end

.end:
    ret
)");

int main(int argc, char *argv[]) {
  // Read the inputs from stdin.
  std::int32_t i;

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

// CHECK: Address range: [[EXE_PATH]]::.dummy_text::is_odd+0x0 --> [[EXE_PATH]]::.dummy_text::is_odd+0xc
// CHECK: Number of executions:
// CHECK-SAME: 7

// .even basic block
// =================

// CHECK: Address range: [[EXE_PATH]]::.dummy_text::is_odd+0xc --> [[EXE_PATH]]::.dummy_text::is_odd+0x13
// CHECK: Number of executions:
// CHECK-SAME: 4

// .odd basic block
// ================

// CHECK: Address range: [[EXE_PATH]]::.dummy_text::is_odd+0x13 --> [[EXE_PATH]]::.dummy_text::is_odd+0x1a
// CHECK: Number of executions:
// CHECK-SAME: 3

// .end basic block
// ================

// CHECK: Address range: [[EXE_PATH]]::.dummy_text::is_odd+0x1a --> [[EXE_PATH]]::.dummy_text::is_odd+0x1b
// CHECK: Number of executions:
// CHECK-SAME: 7
