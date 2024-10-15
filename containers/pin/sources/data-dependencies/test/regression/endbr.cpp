// RUN: g++ %s -o %t.exe -masm=intel

// RUN: nm --numeric-sort %t.exe > %t.symbols

// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -csv_prefix %t -replay -replay:basename %t/pinball -replay:addr_trans -replay:playout -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t > %t.out

// RUN: cat %t.symbols %t.out | FileCheck %s -DEXE_NAME=%basename_t.tmp.exe

extern "C" void set_regs();
extern "C" void func();

#ifdef PIN_TARGET_ARCH_X64

asm(R"(
  .section        .text
  .globl          set_regs

set_regs:
  push %rax
  push %rbx
  push %rcx
  push %rdx
  push %rsi
  push %rdi
  push %rbp

  pop %rbp
  pop %rdi
  pop %rsi
  pop %rdx
  pop %rcx
  pop %rbx
  pop %rax

  .section        .text
  .globl          func

.balign 16; func:
.balign 16; endbr64
.balign 16; ret
)");

#else

asm(R"(
  .section        .text
  .globl          set_regs

set_regs:
  push %eax
  push %ebx
  push %ecx
  push %edx
  push %esi
  push %edi
  push %ebp

  pop %ebp
  pop %edi
  pop %esi
  pop %edx
  pop %ecx
  pop %ebx
  pop %eax

  .section        .text
  .globl          func

.balign 16; func:
.balign 16; endbr32
.balign 16; ret
)");


#endif

int main(int argc, char *argv[]) {
  set_regs();
  func();

  return 0;
}

// clang-format off

// Grab the start address of the 'func' function.
// CHECK: [[#%x,FUNC_ADDR:]] {{.*}} func

// CHECK-NOT: [[EXE_NAME]]+0x[[#FUNC_ADDR]]
