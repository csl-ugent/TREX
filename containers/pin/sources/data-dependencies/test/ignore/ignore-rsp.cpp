// RUN: g++ %s -o %t.exe -masm=intel

// RUN: nm --numeric-sort %t.exe > %t.symbols

// RUN: %sde -log -log:basename %t/pinball -- %t.exe

// RUN: %sde %toolarg -csv_prefix %t -shortcuts -replay -replay:basename %t/pinball -replay:addr_trans -replay:playout -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t > %t.out

// RUN: cat %t.symbols %t.out | FileCheck %s -DEXE_NAME=%basename_t.tmp.exe --check-prefixes=CHECK,NOIGNORE

// RUN: %sde %toolarg -csv_prefix %t.ignore -shortcuts -ignore_rsp -replay -replay:basename %t/pinball -replay:addr_trans -replay:playout -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t.ignore > %t.ignore.out

// RUN: cat %t.symbols %t.ignore.out | FileCheck %s -DEXE_NAME=%basename_t.tmp.exe --check-prefixes=CHECK,IGNORE

extern "C" void func();

#ifdef PIN_TARGET_ARCH_X64

asm(R"(
  .section        .text
  .globl          func

func:

.push:          push            rax
.pop:           pop             rax

                ret
)");

#else

asm(R"(
  .section        .text
  .globl          func

func:

.push:          push            eax
.pop:           pop             eax


                ret
)");


#endif

int main(int argc, char *argv[]) {
  func();

  return 0;
}

// clang-format off

// Grab the relevant addresses.
// CHECK: [[#%x,PUSH_ADDR:]] {{.*}} .push
// CHECK: [[#%x,POP_ADDR:]] {{.*}} .pop

// CHECK: REGISTER DEPENDENCIES
// CHECK: =====================

// NOIGNORE:      Instructions: [[EXE_NAME]]+0x[[#PUSH_ADDR]] <- [[EXE_NAME]]+0x[[#POP_ADDR]]
// NOIGNORE:      Register:
// NOIGNORE-SAME: [[gsp]]

// IGNORE-NOT:    Instructions: [[EXE_NAME]]+0x[[#PUSH_ADDR]] <- [[EXE_NAME]]+0x[[#POP_ADDR]]
