// RUN: g++ %s -o %t.exe -masm=intel

// RUN: nm --numeric-sort %t.exe > %t.symbols

// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -csv_prefix %t -shortcuts -replay -replay:basename %t/pinball -replay:addr_trans -replay:playout -- nullapp >%t.appout
// RUN: pretty-print-csvs.py --prefix=%t > %t.out

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
.balign 16; nop
.balign 16; nop
.balign 16; nop
.balign 16; nop

.balign 16; mov           QWORD PTR [rsi], 1

# movs* auto increments/decrements rdi/rsi, so we need to save and restore it.
.balign 16; push          rdi
.balign 16; movsq       # QWORD PTR [rdi], QWORD PTR [rsi]
.balign 16; pop           rdi

.balign 16; mov           rdx, QWORD PTR [rdi]

.balign 16; nop
.balign 16; nop

.balign 16; ret
)");

#else

// Int with the size of a machine register.
typedef std::int32_t native_int;

asm(R"(
  .section        .text
  .globl          func

.balign 16; func:
.balign 16; push          edi
.balign 16; push          esi
.balign 16; mov           edi, DWORD PTR [esp + 12]
.balign 16; mov           esi, DWORD PTR [esp + 16]

.balign 16; mov           DWORD PTR [esi], 1

# movs* auto increments/decrements edi/esi, so we need to save and restore it.
.balign 16; push          edi
.balign 16; movsd       # DWORD PTR [edi], DWORD PTR [esi]
.balign 16; pop           edi

.balign 16; mov           edx, DWORD PTR [edi]

.balign 16; pop           esi
.balign 16; pop           edi

.balign 16; ret
)");

#endif

extern "C" void func(native_int *dst, native_int *src);

int main(int argc, char *argv[]) {
  native_int x, y;

  std::cout << "Address of x: " << &x << std::endl;
  std::cout << "Address of y: " << &y << std::endl;
  func(&x, &y);

  return 0;
}

// clang-format off

// Grab the address of x and y.
// CHECK: Address of x: 0x[[#%x,X_ADDR:]]
// CHECK: Address of y: 0x[[#%x,Y_ADDR:]]

// Grab the start address of the 'func' function.
// CHECK: [[#%x,FUNC_ADDR:]] {{.*}} func

// CHECK: MEMORY DEPENDENCIES
// CHECK: ===================

// CHECK:      Instructions: [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(4, 16)]] <- [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(6, 16)]]
// CHECK:      Memory address:
// CHECK-SAME: 0x[[#Y_ADDR]]

// CHECK:      Instructions: [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(4, 16)]] <- [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(8, 16)]]
// CHECK:      Memory address:
// CHECK-SAME: 0x[[#X_ADDR]]
