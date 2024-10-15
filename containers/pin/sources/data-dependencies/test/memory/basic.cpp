// RUN: g++ %s -o %t.exe -masm=intel

// RUN: nm --numeric-sort %t.exe > %t.symbols

// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -csv_prefix %t -replay -replay:basename %t/pinball -replay:addr_trans -replay:playout -- nullapp >%t.appout
// RUN: pretty-print-csvs.py --prefix=%t >%t.out

// RUN: cat %t.appout %t.symbols %t.out | \
// RUN: FileCheck %s -DEXE_NAME=%basename_t.tmp.exe

#include <cstdint>
#include <iostream>

#ifdef PIN_TARGET_ARCH_X64

// Int with the size of a machine register.
typedef std::int64_t native_int;

asm(R"(
  .section        .text
  .globl          func

.balign 16; func:
.balign 16;   nop
.balign 16;   nop

.balign 16;   mov    QWORD PTR [rdi], 1

.balign 16;   mov    QWORD PTR [rdi], 2
.balign 16;   mov    rcx, QWORD PTR [rdi]
.balign 16;   mov    rdx, QWORD PTR [rdi]

.balign 16;   mov    QWORD PTR [rdi], 3
.balign 16;   mov    rcx, QWORD PTR [rdi]
.balign 16;   mov    rdx, QWORD PTR [rdi]

.balign 16;   nop

.balign 16;   ret
)");

#else

// Int with the size of a machine register.
typedef std::int32_t native_int;

asm(R"(
  .section        .text
  .globl          func

.balign 16; func:
.balign 16;   push          edi
.balign 16;   mov           edi, DWORD PTR [esp + 8]

.balign 16;   mov           DWORD PTR [edi], 1

.balign 16;   mov           DWORD PTR [edi], 2
.balign 16;   mov           ecx, DWORD PTR [edi]
.balign 16;   mov           edx, DWORD PTR [edi]

.balign 16;   mov           DWORD PTR [edi], 3
.balign 16;   mov           ecx, DWORD PTR [edi]
.balign 16;   mov           edx, DWORD PTR [edi]

.balign 16;   pop           edi

.balign 16;   ret
)");

#endif

extern "C" void func(native_int *ptr);

int main(int argc, char *argv[]) {
  native_int x;

  std::cout << "Address of x: " << &x << std::endl;
  func(&x);

  return 0;
}

// clang-format off

// Grab the address of x.
// CHECK: Address of x: 0x[[#%x,X_ADDR:]]

// Grab the start address of the 'func' function.
// CHECK: [[#%x,FUNC_ADDR:]] {{.*}} func

// CHECK: MEMORY DEPENDENCIES
// CHECK: ===================

// CHECK:      Instructions: [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(3, 16)]] <- [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(4, 16)]]
// CHECK:      Memory address:
// CHECK-SAME: 0x[[#X_ADDR]]

// CHECK:      Instructions: [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(3, 16)]] <- [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(5, 16)]]
// CHECK:      Memory address:
// CHECK-SAME: 0x[[#X_ADDR]]

// CHECK:      Instructions: [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(6, 16)]] <- [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(7, 16)]]
// CHECK:      Memory address:
// CHECK-SAME: 0x[[#X_ADDR]]

// CHECK:      Instructions: [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(6, 16)]] <- [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(8, 16)]]
// CHECK:      Memory address:
// CHECK-SAME: 0x[[#X_ADDR]]
