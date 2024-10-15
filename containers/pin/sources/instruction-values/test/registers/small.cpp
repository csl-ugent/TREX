// RUN: g++ %s -o %t.exe -masm=intel

// RUN: nm --numeric-sort %t.exe > %t.symbols

// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -csv_prefix %t -replay -replay:basename %t/pinball -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t > %t.out

// RUN: cat %t.symbols %t.out | FileCheck %s -DEXE_NAME=%basename_t.tmp.exe \
// RUN: --check-prefixes CHECK

extern "C" void func();

#ifdef PIN_TARGET_ARCH_X64

asm(R"(
  .section        .text
  .globl          func

.balign 16; func:
.balign 16; xor           rcx, rcx

.balign 16; mov           cx, 10
.balign 16; mov           ch, 20
.balign 16; mov           cl, 30

.balign 16; ret
)");

#else

asm(R"(
  .section        .text
  .globl          func

.balign 16; func:
.balign 16; xor           ecx, ecx

.balign 16; mov           cx, 10
.balign 16; mov           ch, 20
.balign 16; mov           cl, 30

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

// CHECK: INSTRUCTIONS
// CHECK: ============

// mov cx, 10
// ----------

// CHECK:      Instruction: [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(1, 16)]]

// CHECK:      Operand [[#]] repr: cx
// CHECK:      Operand [[#]] width:
// CHECK-SAME: 16
// CHECK:      Operand [[#]] is read:
// CHECK-SAME: False
// CHECK:      Operand [[#]] is written:
// CHECK-SAME: True
// CHECK:      Operand [[#]] read values:
// CHECK-SAME: []
// CHECK:      Operand [[#]] written values:
// CHECK-SAME: [0a 00]

// mov ch, 20
// ----------

// CHECK:      Instruction: [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(2, 16)]]

// CHECK:      Operand [[#]] repr: ch
// CHECK:      Operand [[#]] width:
// CHECK-SAME: 8
// CHECK:      Operand [[#]] is read:
// CHECK-SAME: False
// CHECK:      Operand [[#]] is written:
// CHECK-SAME: True
// CHECK:      Operand [[#]] read values:
// CHECK-SAME: []
// CHECK:      Operand [[#]] written values:
// CHECK-SAME: [14]

// mov cl, 30
// ----------

// CHECK:      Instruction: [[EXE_NAME]]+0x[[#FUNC_ADDR + mul(3, 16)]]

// CHECK:      Operand [[#]] repr: cl
// CHECK:      Operand [[#]] width:
// CHECK-SAME: 8
// CHECK:      Operand [[#]] is read:
// CHECK-SAME: False
// CHECK:      Operand [[#]] is written:
// CHECK-SAME: True
// CHECK:      Operand [[#]] read values:
// CHECK-SAME: []
// CHECK:      Operand [[#]] written values:
// CHECK-SAME: [1e]
