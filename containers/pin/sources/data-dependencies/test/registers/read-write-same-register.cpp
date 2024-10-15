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
.balign 16; xor           rax, rax

.balign 16; ret
)");

#else

asm(R"(
  .section        .text
  .globl          func

.balign 16; func:
.balign 16; mov           eax, 1
.balign 16; xor           eax, eax

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

// CHECK-NOT: Instructions: [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(1, 16)]] <- [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(1, 16)]]
