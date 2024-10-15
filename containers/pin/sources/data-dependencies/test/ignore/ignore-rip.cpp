// RUN: g++ %s -o %t.exe -masm=intel

// RUN: nm --numeric-sort %t.exe > %t.symbols

// RUN: %sde -log -log:basename %t/pinball -- %t.exe

// RUN: %sde %toolarg -csv_prefix %t -shortcuts -replay -replay:basename %t/pinball -replay:addr_trans -replay:playout -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t > %t.out

// RUN: cat %t.symbols %t.out | FileCheck %s -DEXE_NAME=%basename_t.tmp.exe --check-prefixes=CHECK,NOIGNORE

// RUN: %sde %toolarg -csv_prefix %t.ignore -shortcuts -ignore_rip -replay -replay:basename %t/pinball -replay:addr_trans -replay:playout -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t.ignore > %t.ignore.out

// RUN: cat %t.symbols %t.ignore.out | FileCheck %s -DEXE_NAME=%basename_t.tmp.exe --check-prefixes=CHECK,IGNORE

extern "C" void func();

#ifdef PIN_TARGET_ARCH_X64

asm(R"(
  .section        .text
  .globl          func

func:

.jmp1:          jmp .jmp2
.jmp2:          jmp .ret

.ret:           ret
)");

#else

asm(R"(
  .section        .text
  .globl          func

func:

.jmp1:          jmp .jmp2
.jmp2:          jmp .ret

.ret:           ret
)");


#endif

int main(int argc, char *argv[]) {
  func();

  return 0;
}

// clang-format off

// Grab the relevant addresses.
// CHECK: [[#%x,JMP1_ADDR:]] {{.*}} .jmp1
// CHECK: [[#%x,JMP2_ADDR:]] {{.*}} .jmp2

// CHECK: REGISTER DEPENDENCIES
// CHECK: =====================

// NOIGNORE:      Instructions: [[EXE_NAME]]+0x[[#JMP1_ADDR]] <- [[EXE_NAME]]+0x[[#JMP2_ADDR]]
// NOIGNORE:      Register:
// NOIGNORE-SAME: [[gip]]

// IGNORE-NOT:    Instructions: [[EXE_NAME]]+0x[[#JMP1_ADDR]] <- [[EXE_NAME]]+0x[[#JMP2_ADDR]]
