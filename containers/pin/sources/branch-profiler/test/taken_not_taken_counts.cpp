// RUN: g++ %s -o %t.exe -masm=intel

// RUN: nm --numeric-sort %t.exe > %t.symbols

// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -csv_prefix %t -replay -replay:basename %t/pinball -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t > %t.out

// RUN: cat %t.symbols %t.out | FileCheck %s -DEXE_NAME=%basename_t.tmp.exe

#include <cstdint>
#include <iostream>

#ifdef PIN_TARGET_ARCH_X64

typedef std::int64_t native_int;

asm(R"(
  .section        .text
  .globl          is_odd

.balign 16; is_odd:
.balign 16;     nop
.balign 16;     nop

.balign 16;     and     rdi, 1
.balign 16;     cmp     rdi, 0
.balign 16;     jne     .odd

.balign 16; .even:
.balign 16;     mov     rax, 0
.balign 16;     jmp     .end

.balign 16; .odd:
.balign 16;     mov     rax, 1
.balign 16;     jmp     .end

.balign 16; .end:
.balign 16;     nop
.balign 16;     ret
)");

#else

typedef std::int32_t native_int;

asm(R"(
  .section        .text
  .globl          is_odd

.balign 16; is_odd:
.balign 16;     push    edi
.balign 16;     mov     edi, DWORD PTR [esp + 8]

.balign 16;     and     edi, 1
.balign 16;     cmp     edi, 0
.balign 16;     jne     .odd

.balign 16; .even:
.balign 16;     mov     eax, 0
.balign 16;     jmp     .end

.balign 16; .odd:
.balign 16;     mov     eax, 1
.balign 16;     jmp     .end

.balign 16; .end:
.balign 16;     pop     edi
.balign 16;     ret
)");

#endif

extern "C" native_int is_odd(native_int n);

int main(int argc, char *argv[]) {
  native_int inputs[] = {1, 3, 2, 4, 6, 5, 8};

  for (native_int &v : inputs) {
    std::cout << is_odd(v) << '\n';
  }

  return 0;
}

// clang-format off

// Grab the start address of the 'is_odd' function.
// CHECK: [[#%x,IS_ODD_ADDR:]] {{.*}} is_odd

// CHECK: BRANCHES
// CHECK: ========

// CHECK:      Instruction: [[EXE_NAME]]+0x[[#IS_ODD_ADDR + mul(4, 16)]]
// CHECK:      Times taken:
// CHECK-SAME: 3{{$}}
// CHECK:      Times not taken:
// CHECK-SAME: 4{{$}}

// CHECK:      Instruction: [[EXE_NAME]]+0x[[#IS_ODD_ADDR + mul(6, 16)]]
// CHECK:      Times taken:
// CHECK-SAME: 4{{$}}
// CHECK:      Times not taken:
// CHECK-SAME: 0{{$}}

// CHECK:      Instruction: [[EXE_NAME]]+0x[[#IS_ODD_ADDR + mul(8, 16)]]
// CHECK:      Times taken:
// CHECK-SAME: 3{{$}}
// CHECK:      Times not taken:
// CHECK-SAME: 0{{$}}
