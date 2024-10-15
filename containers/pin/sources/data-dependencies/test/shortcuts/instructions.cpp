// RUN: g++ %s -o %t.exe -masm=intel

// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -csv_prefix %t -shortcuts -v -replay -replay:basename %t/pinball -replay:addr_trans -replay:playout -- nullapp >%t.appout 2>%t.apperr

// RUN: grep -F '[DEBUG] [MOVOPERANDMAP]' %t.apperr | \
// RUN: FileCheck %s -DEXE_NAME=%basename_t.tmp.exe --check-prefixes CHECK,%arch

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

.balign 16; cmovb         rax, rcx
.balign 16; cmovbe        rax, rcx
.balign 16; cmovl         rax, rcx
.balign 16; cmovle        rax, rcx
.balign 16; cmovnb        rax, rcx
.balign 16; cmovnbe       rax, rcx
.balign 16; cmovnl        rax, rcx
.balign 16; cmovnle       rax, rcx
.balign 16; cmovno        rax, rcx
.balign 16; cmovnp        rax, rcx
.balign 16; cmovns        rax, rcx
.balign 16; cmovnz        rax, rcx
.balign 16; cmovo         rax, rcx
.balign 16; cmovp         rax, rcx
.balign 16; cmovs         rax, rcx
.balign 16; cmovz         rax, rcx
.balign 16; lodsb       # rax, BYTE PTR [rsi]
.balign 16; lodsd       # rax, DWORD PTR [rsi]
.balign 16; lodsq       # rax, QWORD PTR [rsi]
.balign 16; lodsw       # rax, WORD PTR [rsi]
.balign 16; mov           rax, rcx
.balign 16; movaps        xmm0, xmm1
.balign 16; movd          xmm0, ecx
.balign 16; movdqa        xmm0, xmm1
.balign 16; movdqu        xmm0, xmm1
.balign 16; movq          xmm0, xmm1
.balign 16; movsb       # BYTE PTR [rdi], BYTE PTR [rsi]
.balign 16; movsd       # DWORD PTR [rdi], DWORD PTR [rsi]
.balign 16; movsq       # QWORD PTR [rdi], QWORD PTR [rsi]
.balign 16; movsw       # WORD PTR [rdi], WORD PTR [rsi]
.balign 16; movsx         eax, cx
.balign 16; movzx         eax, cx
.balign 16; pop           rax
.balign 16; push          rax
.balign 16; mov rcx, 1; rep lodsb
.balign 16; mov rcx, 1; rep lodsd
.balign 16; mov rcx, 1; rep lodsq
.balign 16; mov rcx, 1; rep lodsw
.balign 16; mov rcx, 1; rep movsb
.balign 16; mov rcx, 1; rep movsd
.balign 16; mov rcx, 1; rep movsq
.balign 16; mov rcx, 1; rep movsw
.balign 16; mov rcx, 1; rep stosb
.balign 16; mov rcx, 1; rep stosd
.balign 16; mov rcx, 1; rep stosq
.balign 16; mov rcx, 1; rep stosw
.balign 16; stosb       # BYTE PTR [rdi], rax
.balign 16; stosd       # DWORD PTR [rdi], rax
.balign 16; stosq       # QWORD PTR [rdi], rax
.balign 16; stosw       # WORD PTR [rdi], rax
.balign 16; vmovdqu     xmm0, xmm1
#.balign 16; vmovdqu8    xmm0, xmm1 NOTE: Requires AVX512VL AVX512BW, which I do not have.
#.balign 16; vmovdqu16   xmm0, xmm1 NOTE: Requires AVX512VL AVX512BW, which I do not have.
#.balign 16; vmovdqu32   xmm0, xmm1 NOTE: Requires AVX512VL AVX512BW, which I do not have.
#.balign 16; vmovdqu64   xmm0, xmm1 NOTE: Requires AVX512VL AVX512BW, which I do not have.
.balign 16; vmovdqa     xmm0, xmm1
#.balign 16; vmovdqa32   xmm0, xmm1 NOTE: Requires AVX512VL AVX512F, which I do not have.
#.balign 16; vmovdqa64   xmm0, xmm1 NOTE: Requires AVX512VL AVX512F, which I do not have.

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

.balign 16; cmovb         eax, ecx
.balign 16; cmovbe        eax, ecx
.balign 16; cmovl         eax, ecx
.balign 16; cmovle        eax, ecx
.balign 16; cmovnb        eax, ecx
.balign 16; cmovnbe       eax, ecx
.balign 16; cmovnl        eax, ecx
.balign 16; cmovnle       eax, ecx
.balign 16; cmovno        eax, ecx
.balign 16; cmovnp        eax, ecx
.balign 16; cmovns        eax, ecx
.balign 16; cmovnz        eax, ecx
.balign 16; cmovo         eax, ecx
.balign 16; cmovp         eax, ecx
.balign 16; cmovs         eax, ecx
.balign 16; cmovz         eax, ecx
.balign 16; lodsb       # eax, BYTE PTR [esi]
.balign 16; lodsd       # eax, DWORD PTR [esi]
.balign 16; nop         # lodsq is not supported on x86
.balign 16; lodsw       # eax, WORD PTR [esi]
.balign 16; mov           eax, ecx
.balign 16; movaps        xmm0, xmm1
.balign 16; movd          xmm0, ecx
.balign 16; movdqa        xmm0, xmm1
.balign 16; movdqu        xmm0, xmm1
.balign 16; movq          xmm0, xmm1
.balign 16; movsb       # BYTE PTR [edi], BYTE PTR [esi]
.balign 16; movsd       # DWORD PTR [edi], DWORD PTR [esi]
.balign 16; nop         # movsq is not supported on x86
.balign 16; movsw       # WORD PTR [edi], WORD PTR [esi]
.balign 16; movsx         eax, cx
.balign 16; movzx         eax, cx
.balign 16; pop           eax
.balign 16; push          eax
.balign 16; mov ecx, 1; rep lodsb
.balign 16; mov ecx, 1; rep lodsd
.balign 16; nop         # rep lodsq is not supported on x86
.balign 16; mov ecx, 1; rep lodsw
.balign 16; mov ecx, 1; rep movsb
.balign 16; mov ecx, 1; rep movsd
.balign 16; nop         # rep movsq is not supported on x86
.balign 16; mov ecx, 1; rep movsw
.balign 16; mov ecx, 1; rep stosb
.balign 16; mov ecx, 1; rep stosd
.balign 16; nop         # rep stosq is not supported on x86
.balign 16; mov ecx, 1; rep stosw
.balign 16; stosb       # BYTE PTR [edi], eax
.balign 16; stosd       # DWORD PTR [edi], eax
.balign 16; nop         # stosq is not supported on x86
.balign 16; stosw       # WORD PTR [edi], eax

.balign 16; pop           esi
.balign 16; pop           edi

.balign 16; ret
)");

#endif

extern "C" void func(native_int *dst, native_int *src);

int main(int argc, char *argv[]) {
  native_int x[128];
  native_int y[128];

  func(&x[0], &y[0]);

  return 0;
}

// clang-format off

// CHECK:      Instruction: cmovb [[gax]], [[gcx]]
// CHECK-NEXT: Pair: dst = [[gax]], src = [[gcx]]

// CHECK:      Instruction: cmovbe [[gax]], [[gcx]]
// CHECK-NEXT: Pair: dst = [[gax]], src = [[gcx]]

// CHECK:      Instruction: cmovl [[gax]], [[gcx]]
// CHECK-NEXT: Pair: dst = [[gax]], src = [[gcx]]

// CHECK:      Instruction: cmovle [[gax]], [[gcx]]
// CHECK-NEXT: Pair: dst = [[gax]], src = [[gcx]]

// CHECK:      Instruction: cmovnb [[gax]], [[gcx]]
// CHECK-NEXT: Pair: dst = [[gax]], src = [[gcx]]

// CHECK:      Instruction: cmovnbe [[gax]], [[gcx]]
// CHECK-NEXT: Pair: dst = [[gax]], src = [[gcx]]

// CHECK:      Instruction: cmovnl [[gax]], [[gcx]]
// CHECK-NEXT: Pair: dst = [[gax]], src = [[gcx]]

// CHECK:      Instruction: cmovnle [[gax]], [[gcx]]
// CHECK-NEXT: Pair: dst = [[gax]], src = [[gcx]]

// CHECK:      Instruction: cmovno [[gax]], [[gcx]]
// CHECK-NEXT: Pair: dst = [[gax]], src = [[gcx]]

// CHECK:      Instruction: cmovnp [[gax]], [[gcx]]
// CHECK-NEXT: Pair: dst = [[gax]], src = [[gcx]]

// CHECK:      Instruction: cmovns [[gax]], [[gcx]]
// CHECK-NEXT: Pair: dst = [[gax]], src = [[gcx]]

// CHECK:      Instruction: cmovnz [[gax]], [[gcx]]
// CHECK-NEXT: Pair: dst = [[gax]], src = [[gcx]]

// CHECK:      Instruction: cmovo [[gax]], [[gcx]]
// CHECK-NEXT: Pair: dst = [[gax]], src = [[gcx]]

// CHECK:      Instruction: cmovp [[gax]], [[gcx]]
// CHECK-NEXT: Pair: dst = [[gax]], src = [[gcx]]

// CHECK:      Instruction: cmovs [[gax]], [[gcx]]
// CHECK-NEXT: Pair: dst = [[gax]], src = [[gcx]]

// CHECK:      Instruction: cmovz [[gax]], [[gcx]]
// CHECK-NEXT: Pair: dst = [[gax]], src = [[gcx]]

// CHECK:      Instruction: lodsb byte ptr ([[gsi]])
// X64-NEXT:   Pair: dst = al, src = *invalid*:([[gsi]] + (*invalid* * 1) + 0)
// x86-NEXT:   Pair: dst = al, src = ds:([[gsi]] + (*invalid* * 1) + 0)

// CHECK:      Instruction: lodsd dword ptr ([[gsi]])
// X64-NEXT:   Pair: dst = eax, src = *invalid*:([[gsi]] + (*invalid* * 1) + 0)
// x86-NEXT:   Pair: dst = eax, src = ds:([[gsi]] + (*invalid* * 1) + 0)

// X64:        Instruction: lodsq qword ptr ([[gsi]])
// X64-NEXT:   Pair: dst = rax, src = *invalid*:([[gsi]] + (*invalid* * 1) + 0)

// CHECK:      Instruction: lodsw word ptr ([[gsi]])
// X64-NEXT:   Pair: dst = ax, src = *invalid*:([[gsi]] + (*invalid* * 1) + 0)
// x86-NEXT:   Pair: dst = ax, src = ds:([[gsi]] + (*invalid* * 1) + 0)

// CHECK:      Instruction: mov [[gax]], [[gcx]]
// CHECK-NEXT: Pair:        dst = [[gax]], src = [[gcx]]

// CHECK:      Instruction: movaps xmm0, xmm1
// CHECK-NEXT: Pair:        dst = xmm0, src = xmm1

// CHECK:      Instruction: movd xmm0, ecx
// CHECK-NEXT: Pair:        dst = xmm0, src = ecx

// CHECK:      Instruction: movdqa xmm0, xmm1
// CHECK-NEXT: Pair:        dst = xmm0, src = xmm1

// CHECK:      Instruction: movdqu xmm0, xmm1
// CHECK-NEXT: Pair:        dst = xmm0, src = xmm1

// CHECK:      Instruction: movq xmm0, xmm1
// CHECK-NEXT: Pair:        dst = xmm0, src = xmm1

// CHECK:      Instruction: movsb byte ptr ([[gdi]]), byte ptr ([[gsi]])
// X64-NEXT:   Pair:        dst = *invalid*:([[gdi]] + (*invalid* * 1) + 0), src = *invalid*:([[gsi]] + (*invalid* * 1) + 0)
// X86-NEXT:   Pair:        dst = es:([[gdi]] + (*invalid* * 1) + 0), src = ds:([[gsi]] + (*invalid* * 1) + 0)

// CHECK:      Instruction: movsd dword ptr ([[gdi]]), dword ptr ([[gsi]])
// X64-NEXT:   Pair:        dst = *invalid*:([[gdi]] + (*invalid* * 1) + 0), src = *invalid*:([[gsi]] + (*invalid* * 1) + 0)
// X86-NEXT:   Pair:        dst = es:([[gdi]] + (*invalid* * 1) + 0), src = ds:([[gsi]] + (*invalid* * 1) + 0)

// X64:        Instruction: movsq qword ptr ([[gdi]]), qword ptr ([[gsi]])
// X64-NEXT:   Pair:        dst = *invalid*:([[gdi]] + (*invalid* * 1) + 0), src = *invalid*:([[gsi]] + (*invalid* * 1) + 0)

// CHECK:      Instruction: movsw word ptr ([[gdi]]), word ptr ([[gsi]])
// X64-NEXT:   Pair:        dst = *invalid*:([[gdi]] + (*invalid* * 1) + 0), src = *invalid*:([[gsi]] + (*invalid* * 1) + 0)
// X86-NEXT:   Pair:        dst = es:([[gdi]] + (*invalid* * 1) + 0), src = ds:([[gsi]] + (*invalid* * 1) + 0)

// CHECK:      Instruction: movsx eax, cx
// CHECK-NEXT: Pair:        dst = eax, src = cx

// CHECK:      Instruction: movzx eax, cx
// CHECK-NEXT: Pair:        dst = eax, src = cx

// CHECK:      Instruction: pop [[gax]]
// X64-NEXT:   Pair:        dst = [[gax]], src = *invalid*:(rsp + (*invalid* * 1) + 0)
// X86-NEXT:   Pair:        dst = [[gax]], src = ss:(esp + (*invalid* * 1) + 0)

// CHECK:      Instruction: push [[gax]]
// X64-NEXT:   Pair:        dst = *invalid*:(rsp + (*invalid* * 1) + 0), src = [[gax]]
// X86-NEXT:   Pair:        dst = ss:(esp + (*invalid* * 1) + 0), src = [[gax]]

// CHECK:      Instruction: rep lodsb byte ptr ([[gsi]])
// X64-NEXT:   Pair: dst = al, src = *invalid*:([[gsi]] + (*invalid* * 1) + 0)
// x86-NEXT:   Pair: dst = al, src = ds:([[gsi]] + (*invalid* * 1) + 0)

// CHECK:      Instruction: rep lodsd dword ptr ([[gsi]])
// X64-NEXT:   Pair: dst = eax, src = *invalid*:([[gsi]] + (*invalid* * 1) + 0)
// x86-NEXT:   Pair: dst = eax, src = ds:([[gsi]] + (*invalid* * 1) + 0)

// X64:        Instruction: rep lodsq qword ptr ([[gsi]])
// X64-NEXT:   Pair: dst = rax, src = *invalid*:([[gsi]] + (*invalid* * 1) + 0)

// CHECK:      Instruction: rep lodsw word ptr ([[gsi]])
// X64-NEXT:   Pair: dst = ax, src = *invalid*:([[gsi]] + (*invalid* * 1) + 0)
// x86-NEXT:   Pair: dst = ax, src = ds:([[gsi]] + (*invalid* * 1) + 0)

// CHECK:      Instruction: rep movsb byte ptr ([[gdi]]), byte ptr ([[gsi]])
// X64-NEXT:   Pair:        dst = *invalid*:([[gdi]] + (*invalid* * 1) + 0), src = *invalid*:([[gsi]] + (*invalid* * 1) + 0)
// X86-NEXT:   Pair:        dst = es:([[gdi]] + (*invalid* * 1) + 0), src = ds:([[gsi]] + (*invalid* * 1) + 0)

// CHECK:      Instruction: rep movsd dword ptr ([[gdi]]), dword ptr ([[gsi]])
// X64-NEXT:   Pair:        dst = *invalid*:([[gdi]] + (*invalid* * 1) + 0), src = *invalid*:([[gsi]] + (*invalid* * 1) + 0)
// X86-NEXT:   Pair:        dst = es:([[gdi]] + (*invalid* * 1) + 0), src = ds:([[gsi]] + (*invalid* * 1) + 0)

// X64:        Instruction: rep movsq qword ptr ([[gdi]]), qword ptr ([[gsi]])
// X64-NEXT:   Pair:        dst = *invalid*:([[gdi]] + (*invalid* * 1) + 0), src = *invalid*:([[gsi]] + (*invalid* * 1) + 0)

// CHECK:      Instruction: rep movsw word ptr ([[gdi]]), word ptr ([[gsi]])
// X64-NEXT:   Pair:        dst = *invalid*:([[gdi]] + (*invalid* * 1) + 0), src = *invalid*:([[gsi]] + (*invalid* * 1) + 0)
// X86-NEXT:   Pair:        dst = es:([[gdi]] + (*invalid* * 1) + 0), src = ds:([[gsi]] + (*invalid* * 1) + 0)

// CHECK:      Instruction: rep stosb byte ptr ([[gdi]])
// X64-NEXT:   Pair: dst = *invalid*:(rdi + (*invalid* * 1) + 0), src = al
// X86-NEXT:   Pair: dst = es:([[gdi]] + (*invalid* * 1) + 0), src = al

// CHECK:      Instruction: rep stosd dword ptr ([[gdi]])
// X64-NEXT:   Pair: dst = *invalid*:(rdi + (*invalid* * 1) + 0), src = eax
// X86-NEXT:   Pair: dst = es:([[gdi]] + (*invalid* * 1) + 0), src = eax

// X64:        Instruction: rep stosq qword ptr ([[gdi]])
// X64-NEXT:   Pair: dst = *invalid*:(rdi + (*invalid* * 1) + 0), src = rax

// CHECK:      Instruction: rep stosw word ptr ([[gdi]])
// X64-NEXT:   Pair: dst = *invalid*:(rdi + (*invalid* * 1) + 0), src = ax
// X86-NEXT:   Pair: dst = es:([[gdi]] + (*invalid* * 1) + 0), src = ax

// CHECK:      Instruction: stosb byte ptr ([[gdi]])
// X64-NEXT:   Pair: dst = *invalid*:(rdi + (*invalid* * 1) + 0), src = al
// X86-NEXT:   Pair: dst = es:([[gdi]] + (*invalid* * 1) + 0), src = al

// CHECK:      Instruction: stosd dword ptr ([[gdi]])
// X64-NEXT:   Pair: dst = *invalid*:(rdi + (*invalid* * 1) + 0), src = eax
// X86-NEXT:   Pair: dst = es:([[gdi]] + (*invalid* * 1) + 0), src = eax

// X64:        Instruction: stosq qword ptr ([[gdi]])
// X64-NEXT:   Pair: dst = *invalid*:(rdi + (*invalid* * 1) + 0), src = rax

// CHECK:      Instruction: stosw word ptr ([[gdi]])
// X64-NEXT:   Pair: dst = *invalid*:(rdi + (*invalid* * 1) + 0), src = ax
// X86-NEXT:   Pair: dst = es:([[gdi]] + (*invalid* * 1) + 0), src = ax

// X64:        Instruction: vmovdqu
// X64-NEXT:   Pair: dst = xmm0, src = xmm1

// COM-X64:        Instruction: vmovdqu8
// COM-X64-NEXT:   Pair: dst = xmm0, src = xmm1

// COM-X64:        Instruction: vmovdqu16
// COM-X64-NEXT:   Pair: dst = xmm0, src = xmm1

// COM-X64:        Instruction: vmovdqu32
// COM-X64-NEXT:   Pair: dst = xmm0, src = xmm1

// COM-X64:        Instruction: vmovdqu64
// COM-X64-NEXT:   Pair: dst = xmm0, src = xmm1

// X64:        Instruction: vmovdqa
// X64-NEXT:   Pair: dst = xmm0, src = xmm1

// COM-X64:        Instruction: vmovdqa32
// COM-X64-NEXT:   Pair: dst = xmm0, src = xmm1

// COM-X64:        Instruction: vmovdqa64
// COM-X64-NEXT:   Pair: dst = xmm0, src = xmm1
