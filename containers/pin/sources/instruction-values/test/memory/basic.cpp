// RUN: g++ %s -o %t.exe -masm=intel

// RUN: nm --numeric-sort %t.exe > %t.symbols

// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -csv_prefix %t -replay -replay:basename %t/pinball -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t > %t.out

// RUN: cat %t.symbols %t.out | FileCheck %s -DEXE_NAME=%basename_t.tmp.exe \
// RUN: --check-prefixes CHECK,%arch

#include <cstdint>

#ifdef PIN_TARGET_ARCH_X64

typedef std::int64_t native_int;

asm(R"(
  .section        .text
  .globl          func

.balign 16; func:
.balign 16; push          rdi
.balign 16; nop

.balign 16; mov           qword ptr [rdi], 0x10203040
.balign 16; mov           rcx, qword ptr [rdi]

.balign 16; mov           rcx, 3
.balign 16; .begin_loop:
.balign 16; add           qword ptr [rdi], 1
.balign 16; loop          .begin_loop

.balign 16; pop           rdi
.balign 16; ret
)");

#else

typedef std::int32_t native_int;

asm(R"(
  .section        .text
  .globl          func

.balign 16; func:
.balign 16; push          edi
.balign 16; mov           edi, dword ptr [esp + 8]

.balign 16; mov           dword ptr [edi], 0x10203040
.balign 16; mov           ecx, dword ptr [edi]

.balign 16; mov           ecx, 3
.balign 16; .begin_loop:
.balign 16; add           dword ptr [edi], 1
.balign 16; loop          .begin_loop

.balign 16; pop           edi
.balign 16; ret
)");

#endif

extern "C" void func(native_int *p);

int main(int argc, char *argv[]) {
  native_int p;
  func(&p);

  return 0;
}

// clang-format off

// Grab the start address of the 'func' function.
// CHECK: [[#%x,FUNC_ADDR:]] {{.*}} func

// CHECK: INSTRUCTIONS
// CHECK: ============

// mov qword ptr [rdi], 0x10203040
// ---------------------------

// CHECK:      Instruction: [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(2, 16)]]

// X64:        Operand [[#]] repr: *invalid*:(rdi + (*invalid* * 1) + 0)
// X86:        Operand [[#]] repr: ds:(edi + (*invalid* * 1) + 0)
// CHECK:      Operand [[#]] width:
// CHECK-SAME: [[#bitwidth]]
// CHECK:      Operand [[#]] is read:
// CHECK-SAME: False
// CHECK:      Operand [[#]] is written:
// CHECK-SAME: True
// CHECK:      Operand [[#]] read values:
// CHECK-SAME: []
// CHECK:      Operand [[#]] written values:
// X64-SAME:   [40 30 20 10 00 00 00 00]
// X86-SAME:   [40 30 20 10]

// mov rcx, qword ptr [rdi]
// ------------

// CHECK:      Instruction: [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(3, 16)]]

// X64:        Operand [[#]] repr: *invalid*:(rdi + (*invalid* * 1) + 0)
// X86:        Operand [[#]] repr: ds:(edi + (*invalid* * 1) + 0)
// CHECK:      Operand [[#]] width:
// CHECK-SAME: [[#bitwidth]]
// CHECK:      Operand [[#]] is read:
// CHECK-SAME: True
// CHECK:      Operand [[#]] is written:
// CHECK-SAME: False
// CHECK:      Operand [[#]] read values:
// X64-SAME:   [40 30 20 10 00 00 00 00]
// X86-SAME:   [40 30 20 10]
// CHECK:      Operand [[#]] written values:
// CHECK-SAME: []

// add qword ptr [rdi], 1
// ----------

// CHECK:      Instruction: [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(5, 16)]]

// X64:        Operand [[#]] repr: *invalid*:(rdi + (*invalid* * 1) + 0)
// X86:        Operand [[#]] repr: ds:(edi + (*invalid* * 1) + 0)
// CHECK:      Operand [[#]] width:
// CHECK-SAME: [[#bitwidth]]
// CHECK:      Operand [[#]] is read:
// CHECK-SAME: True
// CHECK:      Operand [[#]] is written:
// CHECK-SAME: True
// CHECK:      Operand [[#]] read values:
// X64-SAME:   [40 30 20 10 00 00 00 00,41 30 20 10 00 00 00 00,42 30 20 10 00 00 00 00]
// X86-SAME:   [40 30 20 10,41 30 20 10,42 30 20 10]
// CHECK:      Operand [[#]] written values:
// X64-SAME:   [41 30 20 10 00 00 00 00,42 30 20 10 00 00 00 00,43 30 20 10 00 00 00 00]
// X86-SAME:   [41 30 20 10,42 30 20 10,43 30 20 10]
