// RUN: g++ %s -o %t.exe -masm=intel

// RUN: nm --numeric-sort %t.exe > %t.symbols

// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -csv_prefix %t -replay -replay:basename %t/pinball -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t > %t.out

// RUN: cat %t.symbols %t.out | FileCheck %s -DEXE_NAME=%basename_t.tmp.exe \
// RUN: --check-prefixes CHECK,%arch

#include <cstdint>

extern "C" void func(uint8_t *data);

#ifdef PIN_TARGET_ARCH_X64

asm(R"(
  .section        .text
  .globl          func

.balign 16; func:
.balign 16; nop
.balign 16; nop

.balign 16; movdqu      xmm0, xmmword ptr [rdi]

.balign 16; nop
.balign 16; ret
)");

#else

asm(R"(
  .section        .text
  .globl          func

.balign 16; func:
.balign 16; push        edi
.balign 16; mov         edi, dword ptr [esp + 8]

.balign 16; movdqu      xmm0, xmmword ptr [edi]

.balign 16; pop         edi
.balign 16; ret
)");

#endif

int main(int argc, char *argv[]) {
  uint8_t data[] = {
      0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70,
      0x80, 0x90, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0,
  };

  func(data);
  return 0;
}

// clang-format off

// Grab the start address of the 'func' function.
// CHECK: [[#%x,FUNC_ADDR:]] {{.*}} func

// CHECK: INSTRUCTIONS
// CHECK: ============

// movdqu xmm0, xmmword ptr [rdi]
// ------------------------------

// CHECK:      Instruction: [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(2, 16)]]

// CHECK       Operand [[#]] repr: xmm0
// CHECK:      Operand [[#]] width:
// CHECK-SAME: 128
// CHECK:      Operand [[#]] is read:
// CHECK-SAME: False
// CHECK:      Operand [[#]] is written:
// CHECK-SAME: True
// CHECK:      Operand [[#]] read values:
// CHECK-SAME: []
// CHECK:      Operand [[#]] written values:
// CHECK-SAME: [00 10 20 30 40 50 60 70 80 90 a0 b0 c0 d0 e0 f0]

// X64:        Operand [[#]] repr: *invalid*:(rdi + (*invalid* * 1) + 0)
// X86:        Operand [[#]] repr: ds:(edi + (*invalid* * 1) + 0)
// CHECK:      Operand [[#]] width:
// CHECK-SAME: 128
// CHECK:      Operand [[#]] is read:
// CHECK-SAME: True
// CHECK:      Operand [[#]] is written:
// CHECK-SAME: False
// CHECK:      Operand [[#]] read values:
// CHECK-SAME: [00 10 20 30 40 50 60 70 80 90 a0 b0 c0 d0 e0 f0]
// CHECK:      Operand [[#]] written values:
// CHECK-SAME: []
