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

.balign 16; mov           rax, 2
.balign 16; mov           rcx, rax
.balign 16; mov           rdx, rax

.balign 16; mov           rax, 3
.balign 16; mov           rcx, rax
.balign 16; mov           rdx, rax

.balign 16; ret
)");

#else

asm(R"(
  .section        .text
  .globl          func

.balign 16; func:
.balign 16; mov           eax, 1

.balign 16; mov           eax, 2
.balign 16; mov           ecx, eax
.balign 16; mov           edx, eax

.balign 16; mov           eax, 3
.balign 16; mov           ecx, eax
.balign 16; mov           edx, eax

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

// CHECK:      Instructions: [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(1, 16)]] <- [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(2, 16)]]
// CHECK:      Register:
// CHECK-SAME: [[gax]]

// CHECK:      Instructions: [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(1, 16)]] <- [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(3, 16)]]
// CHECK:      Register:
// CHECK-SAME: [[gax]]

// CHECK:      Instructions: [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(4, 16)]] <- [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(5, 16)]]
// CHECK:      Register:
// CHECK-SAME: [[gax]]

// CHECK:      Instructions: [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(4, 16)]] <- [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(6, 16)]]
// CHECK:      Register:
// CHECK-SAME: [[gax]]
