// RUN: g++ %s -o %t.exe -masm=intel

// RUN: nm --numeric-sort %t.exe > %t.symbols

// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -csv_prefix %t -shortcuts -replay -replay:basename %t/pinball -replay:addr_trans -replay:playout -- nullapp >%t.appout
// RUN: pretty-print-csvs.py --prefix=%t >%t.out

// RUN: cat %t.appout %t.symbols %t.out | \
// RUN: FileCheck %s -DEXE_NAME=%basename_t.tmp.exe

#include <cstdint>
#include <iostream>

// REQUIRES: x64

asm(R"(
  .section        .text
  .globl          func

func:

.prod0:             inc             rsi
                    mov             byte ptr [rdi+0], sil
.prod1:             inc             rsi
                    mov             byte ptr [rdi+1], sil
.prod2:             inc             rsi
                    mov             byte ptr [rdi+2], sil
.prod3:             inc             rsi
                    mov             byte ptr [rdi+3], sil
.prod4:             inc             rsi
                    mov             byte ptr [rdi+4], sil
.prod5:             inc             rsi
                    mov             byte ptr [rdi+5], sil
.prod6:             inc             rsi
                    mov             byte ptr [rdi+6], sil
.prod7:             inc             rsi
                    mov             byte ptr [rdi+7], sil

                    mov             rax, qword ptr [rdi]

.cons:              inc             rax

                    ret
)");

extern "C" void func(std::int64_t* buf, int counter);

int main(int argc, char *argv[]) {
    std::int64_t buf = 0;

    std::cout << "Address of buf: " << &buf << std::endl;

    func(&buf, 0);

    return 0;
}

// clang-format off

// Grab the address of 'buf'.
// CHECK: Address of buf: 0x[[#%x,BUF_ADDR:]]

// Grab the address of interesting instructions.
// CHECK: [[#%x,PROD0_ADDR:]] {{.*}} .prod0
// CHECK: [[#%x,PROD1_ADDR:]] {{.*}} .prod1
// CHECK: [[#%x,PROD2_ADDR:]] {{.*}} .prod2
// CHECK: [[#%x,PROD3_ADDR:]] {{.*}} .prod3
// CHECK: [[#%x,PROD4_ADDR:]] {{.*}} .prod4
// CHECK: [[#%x,PROD5_ADDR:]] {{.*}} .prod5
// CHECK: [[#%x,PROD6_ADDR:]] {{.*}} .prod6
// CHECK: [[#%x,PROD7_ADDR:]] {{.*}} .prod7
// CHECK: [[#%x,CONS_ADDR:]] {{.*}} .cons

// CHECK: REGISTER DEPENDENCIES
// CHECK: =====================

// CHECK:      virtual-instructions+0x[[#%x,VIRT_OFFSET:]] <- [[EXE_NAME]]+0x[[#CONS_ADDR]]
// CHECK:      Register:
// CHECK-SAME: rax

// CHECK: MEMORY DEPENDENCIES
// CHECK: ===================

// CHECK:      [[EXE_NAME]]+0x[[#PROD0_ADDR]] <- virtual-instructions+0x[[#VIRT_OFFSET]]
// CHECK:      Memory address:
// CHECK-SAME: [[#BUF_ADDR + 0]]

// CHECK:      [[EXE_NAME]]+0x[[#PROD1_ADDR]] <- virtual-instructions+0x[[#VIRT_OFFSET]]
// CHECK:      Memory address:
// CHECK-SAME: [[#BUF_ADDR + 1]]

// CHECK:      [[EXE_NAME]]+0x[[#PROD2_ADDR]] <- virtual-instructions+0x[[#VIRT_OFFSET]]
// CHECK:      Memory address:
// CHECK-SAME: [[#BUF_ADDR + 2]]

// CHECK:      [[EXE_NAME]]+0x[[#PROD3_ADDR]] <- virtual-instructions+0x[[#VIRT_OFFSET]]
// CHECK:      Memory address:
// CHECK-SAME: [[#BUF_ADDR + 3]]

// CHECK:      [[EXE_NAME]]+0x[[#PROD4_ADDR]] <- virtual-instructions+0x[[#VIRT_OFFSET]]
// CHECK:      Memory address:
// CHECK-SAME: [[#BUF_ADDR + 4]]

// CHECK:      [[EXE_NAME]]+0x[[#PROD5_ADDR]] <- virtual-instructions+0x[[#VIRT_OFFSET]]
// CHECK:      Memory address:
// CHECK-SAME: [[#BUF_ADDR + 5]]

// CHECK:      [[EXE_NAME]]+0x[[#PROD6_ADDR]] <- virtual-instructions+0x[[#VIRT_OFFSET]]
// CHECK:      Memory address:
// CHECK-SAME: [[#BUF_ADDR + 6]]

// CHECK:      [[EXE_NAME]]+0x[[#PROD7_ADDR]] <- virtual-instructions+0x[[#VIRT_OFFSET]]
// CHECK:      Memory address:
// CHECK-SAME: [[#BUF_ADDR + 7]]
