// RUN: g++ %s -o %t.exe -masm=intel

// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -output %t.log -replay -replay:basename %t/pinball -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t.log --log_file=%t.log | \
// RUN: FileCheck %s -DEXE_PATH=%t.exe

#include <cstdint>
#include <iostream>

#ifdef PIN_TARGET_ARCH_X64

asm(R"(
    .section        .text
    .globl          func
    .type           func, @function

.balign 16; func:
.balign 16;    push             rsi
.balign 16;    push             rdi
.balign 16;    sub              rsp, 128

.balign 16;    lock cmpxchg     qword ptr [rsp + 8], rcx

.balign 16;    mov              rcx, 1
.balign 16;    lea              rsi, [rsp + 8]
.balign 16;    lea              rdi, [rsp + 16]
.balign 16;    rep movsb

.balign 16;    mov              rcx, 1
.balign 16;    lea              rsi, [rsp + 8]
.balign 16;    lea              rdi, [rsp + 16]
.balign 16;    repne cmpsb

.balign 16;    data16 nop

.balign 16;    add              rsp, 128
.balign 16;    pop              rdi
.balign 16;    pop              rsi

.balign 16;    ret
)");

#else

asm(R"(
    .section        .text
    .globl          func
    .type           func, @function

.balign 16; func:
.balign 16;    push             esi
.balign 16;    push             edi
.balign 16;    sub              esp, 128

.balign 16;    lock cmpxchg     dword ptr [esp + 8], ecx

.balign 16;    mov              ecx, 1
.balign 16;    lea              esi, [esp + 8]
.balign 16;    lea              edi, [esp + 16]
.balign 16;    rep movsb

.balign 16;    mov              ecx, 1
.balign 16;    lea              esi, [esp + 8]
.balign 16;    lea              edi, [esp + 16]
.balign 16;    repne cmpsb

.balign 16;    data16 nop

.balign 16;    add              esp, 128
.balign 16;    pop              edi
.balign 16;    pop              esi

.balign 16;    ret
)");

#endif

extern "C" void func();

int main(int argc, char *argv[]) {
    func();

    return 0;
}

// clang-format off

// CHECK: INSTRUCTIONS
// CHECK: ============

// CHECK:      Address:     [[EXE_PATH]]::.dummy_text::func+0x[[#%x,mul(3,16)]]
// CHECK:      Opcode:
// CHECK-SAME: lock cmpxchg
// CHECK:      Operands:
// CHECK-SAME: {{[qd]}}word ptr [{{[re]}}sp+0x8], {{[re]}}cx

// CHECK:      Address:     [[EXE_PATH]]::.dummy_text::func+0x[[#%x,mul(7,16)]]
// CHECK:      Opcode:
// CHECK-SAME: rep movsb
// CHECK:      Operands:
// CHECK-SAME: {{$}}

// CHECK:      Address:     [[EXE_PATH]]::.dummy_text::func+0x[[#%x,mul(11,16)]]
// CHECK:      Opcode:
// CHECK-SAME: repne cmpsb
// CHECK:      Operands:
// CHECK-SAME: {{$}}

// CHECK:      Address:     [[EXE_PATH]]::.dummy_text::func+0x[[#%x,mul(12,16)]]
// CHECK:      Opcode:
// CHECK-SAME: data16 nop
// CHECK:      Operands:
// CHECK-SAME: {{$}}
