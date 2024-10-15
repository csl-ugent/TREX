// RUN: g++ %s -o %t.exe -masm=intel

// RUN: nm --numeric-sort %t.exe > %t.symbols

// RUN: %sde -log -log:basename %t/pinball -- %t.exe

// RUN: %sde %toolarg -csv_prefix %t -shortcuts -replay -replay:basename %t/pinball -replay:addr_trans -replay:playout -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t > %t.out

// RUN: cat %t.symbols %t.out | FileCheck %s -DEXE_NAME=%basename_t.tmp.exe --check-prefixes=CHECK,NOIGNORE

// RUN: %sde %toolarg -csv_prefix %t.ignore -shortcuts -ignore_rbp_stack_memory -replay -replay:basename %t/pinball -replay:addr_trans -replay:playout -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t.ignore > %t.ignore.out

// RUN: cat %t.symbols %t.ignore.out | FileCheck %s -DEXE_NAME=%basename_t.tmp.exe --check-prefixes=CHECK,IGNORE

#include <cstdint>

extern "C" void func(std::int64_t*);

std::int64_t G;

#ifdef PIN_TARGET_ARCH_X64

asm(R"(
  .section        .text
  .globl          func

func:
                push            rbp

.frag1A:        mov             rbp, 10
.frag1B:        inc             rbp

                mov             rbp, rdi
                inc             rbp
.frag2A:        dec             rbp
.frag2B:        inc             QWORD PTR [rbp]

                push            rax
                mov             rbp, rsp
                inc             rbp
.frag3A:        dec             rbp
.frag3B:        inc             QWORD PTR [rbp]
                pop             rax

                pop             rbp
                ret
)");

#else

asm(R"(
  .section        .text
  .globl          func

func:
                push            ebp

.frag1A:        mov             ebp, 10
.frag1B:        inc             ebp

                mov             ebp, [rsp+8]
                inc             ebp
.frag2A:        dec             ebp
.frag2B:        inc             DWORD PTR [ebp]

                push            eax
                mov             ebp, esp
                inc             ebp
.frag3A:        dec             ebp
.frag3B:        inc             DWORD PTR [ebp]
                pop             eax

                pop             ebp
                ret
)");


#endif

int main(int argc, char *argv[]) {
  func(&G);

  return 0;
}

// clang-format off

// Grab the relevant addresses.
// CHECK: [[#%x,FRAG1A_ADDR:]] {{.*}} .frag1A
// CHECK: [[#%x,FRAG1B_ADDR:]] {{.*}} .frag1B
// CHECK: [[#%x,FRAG2A_ADDR:]] {{.*}} .frag2A
// CHECK: [[#%x,FRAG2B_ADDR:]] {{.*}} .frag2B
// CHECK: [[#%x,FRAG3A_ADDR:]] {{.*}} .frag3A
// CHECK: [[#%x,FRAG3B_ADDR:]] {{.*}} .frag3B

// CHECK: REGISTER DEPENDENCIES
// CHECK: =====================

// CHECK:      Instructions: [[EXE_NAME]]+0x[[#FRAG1A_ADDR]] <- [[EXE_NAME]]+0x[[#FRAG1B_ADDR]]
// CHECK:      Register:
// CHECK-SAME: [[gbp]]

// CHECK:      Instructions: [[EXE_NAME]]+0x[[#FRAG2A_ADDR]] <- [[EXE_NAME]]+0x[[#FRAG2B_ADDR]]
// CHECK:      Register:
// CHECK-SAME: [[gbp]]

// NOIGNORE:      Instructions: [[EXE_NAME]]+0x[[#FRAG3A_ADDR]] <- [[EXE_NAME]]+0x[[#FRAG3B_ADDR]]
// NOIGNORE:      Register:
// NOIGNORE-SAME: [[gbp]]

// IGNORE-NOT:    Instructions: [[EXE_NAME]]+0x[[#FRAG3A_ADDR]] <- [[EXE_NAME]]+0x[[#FRAG3B_ADDR]]
