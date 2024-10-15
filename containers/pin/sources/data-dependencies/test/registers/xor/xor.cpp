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

func:

                push        rdi

.prod:          mov         rdi, 10
.cons:          xor         rdi, rdi

                pop         rdi

                ret
)");

#else

asm(R"(
  .section        .text
  .globl          func

func:

                push        edi

.prod:          mov         edi, 10
.cons:          xor         edi, edi

                pop         edi

                ret
)");


#endif

int main(int argc, char *argv[]) {
  func();

  return 0;
}

// clang-format off

// Grab the relevant addresses.
// CHECK: [[#%x,PROD_ADDR:]] {{.*}} .prod
// CHECK: [[#%x,CONS_ADDR:]] {{.*}} .cons

// CHECK: REGISTER DEPENDENCIES
// CHECK: =====================

// CHECK-NOT:      Instructions: [[EXE_NAME]]+0x[[#PROD_ADDR]] <- [[EXE_NAME]]+0x[[#CONS_ADDR]]
