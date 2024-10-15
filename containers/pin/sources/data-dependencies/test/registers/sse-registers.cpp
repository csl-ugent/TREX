// RUN: g++ %s -o %t.exe -masm=intel

// RUN: nm --numeric-sort %t.exe > %t.symbols

// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -csv_prefix %t -replay -replay:basename %t/pinball -replay:addr_trans -replay:playout -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t > %t.out

// RUN: cat %t.symbols %t.out | FileCheck %s -DEXE_NAME=%basename_t.tmp.exe

extern "C" void func();

#ifdef PIN_TARGET_ARCH_X64

asm(R"(
  .section        .text
  .globl          func

.balign 16; func:
.balign 16; mov           rax, 1

.balign 16; movq          xmm0, rax

.balign 16; movq          xmm0, rax
.balign 16; movq          xmm1, xmm0
.balign 16; movq          xmm2, xmm0

.balign 16; movq          xmm0, rax
.balign 16; movq          xmm1, xmm0
.balign 16; movq          xmm2, xmm0

.balign 16; ret
)");

#else

asm(R"(
  .section        .text
  .globl          func

.balign 16; func:
.balign 16; mov           eax, 1

.balign 16; movd          xmm0, eax

.balign 16; movd          xmm0, eax
.balign 16; movq          xmm1, xmm0
.balign 16; movq          xmm2, xmm0

.balign 16; movd          xmm0, eax
.balign 16; movq          xmm1, xmm0
.balign 16; movq          xmm2, xmm0

.balign 16; ret
)");

#endif

int main(int argc, char *argv[]) {
  func();

  return 0;
}

// clang-format off

// Grab the start address of the 'func' function.
// CHECK: [[#%x,FUNC_ADDR:]] {{.*}} func

// CHECK: REGISTER DEPENDENCIES
// CHECK: =====================

// CHECK:      Instructions: [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(2, 16)]] <- [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(3, 16)]]
// CHECK:      Register:
// CHECK-SAME: ymm0

// CHECK:      Instructions: [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(2, 16)]] <- [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(4, 16)]]
// CHECK:      Register:
// CHECK-SAME: ymm0

// CHECK:      Instructions: [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(5, 16)]] <- [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(6, 16)]]
// CHECK:      Register:
// CHECK-SAME: ymm0

// CHECK:      Instructions: [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(5, 16)]] <- [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(7, 16)]]
// CHECK:      Register:
// CHECK-SAME: ymm0
