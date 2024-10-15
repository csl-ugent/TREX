// RUN: g++ %s -o %t.exe -masm=intel

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
    and             rdi, 1          # 0x0
    cmp             rdi, 0          # 0x4
    jne             .odd            # 0x8
.even:
    mov             rax, 0          # 0xa
    jmp             .end            # 0x11
.odd:
    mov             rax, 1          # 0x13
    jmp             .end            # 0x1a
.end:
    ret                             # 0x1c
)");

int main(int argc, char *argv[]) {
    for (std::int64_t i = 0; i < 10; ++i) {
        std::cout << is_odd(i) << std::endl;
    }

    return 0;
}

// clang-format off

// CHECK: INSTRUCTIONS
// CHECK: ============

// CHECK: Address:     [[EXE_PATH]]::.dummy_text::is_odd+0x0
// CHECK: Instruction:
// CHECK-SAME: and rdi, 0x1

// CHECK: Address:     [[EXE_PATH]]::.dummy_text::is_odd+0x4
// CHECK: Instruction:
// CHECK-SAME: cmp rdi, 0x0

// CHECK: Address:     [[EXE_PATH]]::.dummy_text::is_odd+0x8
// CHECK: Instruction:
// CHECK-SAME: jnz 0x[[#%x,]]

// CHECK: Address:     [[EXE_PATH]]::.dummy_text::is_odd+0xa
// CHECK: Instruction:
// CHECK-SAME: mov rax, 0x0

// CHECK: Address:     [[EXE_PATH]]::.dummy_text::is_odd+0x11
// CHECK: Instruction:
// CHECK-SAME: jmp 0x[[#%x,]]

// CHECK: Address:     [[EXE_PATH]]::.dummy_text::is_odd+0x13
// CHECK: Instruction:
// CHECK-SAME: mov rax, 0x1

// CHECK: Address:     [[EXE_PATH]]::.dummy_text::is_odd+0x1a
// CHECK: Instruction:
// CHECK-SAME: jmp 0x[[#%x,]]

// CHECK: Address:     [[EXE_PATH]]::.dummy_text::is_odd+0x1c
// CHECK: Instruction:
// CHECK-SAME: ret
