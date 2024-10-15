// clang-format off

// RUN: g++ %s -o %t.exe -masm=intel

// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -output %t.log -replay -replay:basename %t/pinball -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t.log --log_file=%t.log | FileCheck %s -DEXE_PATH=%t.exe --check-prefixes CHECK,NOLIM

// RUN: %sde -log -log:basename %t.limit/pinball -- %t.exe
// RUN: %sde %toolarg -output %t.limit.log -instruction_values_limit 2 -replay -replay:basename %t.limit/pinball -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t.limit.log --log_file=%t.limit.log | FileCheck %s -DEXE_PATH=%t.exe --check-prefixes CHECK,LIMIT

// XFAIL: x64

// clang-format on

#include <cstdint>

// Copies 'len' elements from array 'from' to array 'to'.
// to:   [esp + 8]
// from: [esp + 12]
// len:  [esp + 16]
extern "C" void my_memcopy(std::int32_t *to, std::int32_t *from,
                           std::int32_t len);

asm(R"(
  .intel_syntax   noprefix
  .globl          my_memcopy
  .type           my_memcopy, @function

my_memcopy:
  .cfi_startproc

  push            ebx

  .cfi_def_cfa_offset 8
  .cfi_offset 3, -8

  mov             ecx, DWORD PTR 16[esp]
  test            ecx, ecx
  jle             .end

  mov             eax, DWORD PTR 12[esp]
  mov             edx, DWORD PTR 8[esp]
  lea             ebx, [eax+ecx*4]

.copy_loop:
  mov             ecx, DWORD PTR [eax]
  add             eax, 4
  add             edx, 4
  mov             DWORD PTR -4[edx], ecx
  cmp             eax, ebx
  jne             .copy_loop

.end:
  pop   ebx

  .cfi_restore 3
  .cfi_def_cfa_offset 4

  ret

  .cfi_endproc
)");

int main(int argc, char *argv[]) {
  std::int32_t from[4] = {1, 2, 3, 1};
  std::int32_t to[4] = {};

  my_memcopy(to, from, 4);

  return 0;
}

// clang-format off

// CHECK: STATIC INSTRUCTIONS
// CHECK: ===================

// CHECK:      Address:        [[EXE_PATH]]::.dummy_text::my_memcopy+0x14 (0x[[#%x,READ_ADDR:]])
// CHECK-NEXT: Disassembly:    mov ecx, dword ptr [eax]
// NOLIM-NEXT: Read values:    [01 00 00 00, 02 00 00 00, 03 00 00 00]
// LIMIT-NEXT: Read values:    [01 00 00 00, 02 00 00 00]
// CHECK-NEXT: Written values: []
// CHECK: Read values entropy:
// CHECK-SAME: 0.14828
// CHECK: Written values entropy:
// CHECK-SAME: 0{{$}}
// CHECK: Number of bytes read:
// CHECK-SAME: 16{{$}}
// CHECK: Number of bytes written:
// CHECK-SAME: 0{{$}}

// CHECK:      Address:        [[EXE_PATH]]::.dummy_text::my_memcopy+0x1c (0x[[#%x,WRITE_ADDR:]])
// CHECK-NEXT: Disassembly:    mov dword ptr [edx-0x4], ecx
// CHECK-NEXT: Read values:    []
// NOLIM-NEXT: Written values: [01 00 00 00, 02 00 00 00, 03 00 00 00]
// LIMIT-NEXT: Written values: [01 00 00 00, 02 00 00 00]
// CHECK: Read values entropy:
// CHECK-SAME: 0{{$}}
// CHECK: Written values entropy:
// CHECK-SAME: 0.14828
// CHECK: Number of bytes read:
// CHECK-SAME: 0{{$}}
// CHECK: Number of bytes written:
// CHECK-SAME: 16{{$}}

// clang-format on
