// RUN: g++ %s -o %t.exe -masm=intel

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
    mov             eax, DWORD PTR 4[esp]   # 0x0
    and             eax, 1                  # 0x4
    cmp             eax, 0                  # 0x7
    jne             .odd                    # 0xa
.even:
    mov             eax, 0                  # 0xc
    jmp             .end                    # 0x11
.odd:
    mov             eax, 1                  # 0x13
    jmp             .end                    # 0x18
.end:
    ret                                     # 0x1a
)");

int main(int argc, char *argv[]) {
    for (std::int32_t i = 0; i < 10; ++i) {
        std::cout << is_odd(i) << std::endl;
    }

    return 0;
}

// clang-format off

// CHECK: INSTRUCTIONS
// CHECK: ============

// CHECK: Address:     [[EXE_PATH]]::.dummy_text::is_odd+0x0
// CHECK: Instruction:
// CHECK-SAME: mov eax, dword ptr [esp+0x4]

// CHECK: Address:     [[EXE_PATH]]::.dummy_text::is_odd+0x4
// CHECK: Instruction:
// CHECK-SAME: and eax, 0x1

// CHECK: Address:     [[EXE_PATH]]::.dummy_text::is_odd+0x7
// CHECK: Instruction:
// CHECK-SAME: cmp eax, 0x0

// CHECK: Address:     [[EXE_PATH]]::.dummy_text::is_odd+0xa
// CHECK: Instruction:
// CHECK-SAME: jnz 0x[[#%x,]]

// CHECK: Address:     [[EXE_PATH]]::.dummy_text::is_odd+0xc
// CHECK: Instruction:
// CHECK-SAME: mov eax, 0x0

// CHECK: Address:     [[EXE_PATH]]::.dummy_text::is_odd+0x11
// CHECK: Instruction:
// CHECK-SAME: jmp 0x[[#%x,]]

// CHECK: Address:     [[EXE_PATH]]::.dummy_text::is_odd+0x13
// CHECK: Instruction:
// CHECK-SAME: mov eax, 0x1

// CHECK: Address:     [[EXE_PATH]]::.dummy_text::is_odd+0x18
// CHECK: Instruction:
// CHECK-SAME: jmp 0x[[#%x,]]

// CHECK: Address:     [[EXE_PATH]]::.dummy_text::is_odd+0x1a
// CHECK: Instruction:
// CHECK-SAME: ret
