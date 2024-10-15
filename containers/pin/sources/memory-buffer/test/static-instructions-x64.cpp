// clang-format off

// RUN: g++ %s -o %t.exe -masm=intel

// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -output %t.log -replay -replay:basename %t/pinball -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t.log --log_file=%t.log | FileCheck %s -DEXE_PATH=%t.exe --check-prefixes CHECK,NOLIM

// RUN: %sde -log -log:basename %t.limit/pinball -- %t.exe
// RUN: %sde %toolarg -output %t.limit.log -instruction_values_limit 2 -replay -replay:basename %t.limit/pinball -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t.limit.log --log_file=%t.limit.log | FileCheck %s -DEXE_PATH=%t.exe --check-prefixes CHECK,LIMIT

// XFAIL: x86

// clang-format on

#include <cstdint>

// Copies 'len' elements from array 'from' to array 'to'.
// to:   rdi
// from: rsi
// len:  rdx
extern "C" void my_memcopy(std::int64_t *to, std::int64_t *from,
                           std::int64_t len);

asm(R"(
  .intel_syntax   noprefix
  .globl          my_memcopy
  .type           my_memcopy, @function

my_memcopy:
  .cfi_startproc

  test            rdx, rdx
  jle .end

  lea             rcx, 0[0 + rdx*8]
  xor             rax, rax

.copy_loop:
  mov             rdx, QWORD PTR [rsi+rax]
  mov             QWORD PTR [rdi+rax], rdx
  add             rax, 8
  cmp             rcx, rax
  jne             .copy_loop

.end:
  ret

  .cfi_endproc
)");

int main(int argc, char *argv[]) {
  std::int64_t from[4] = {1, 2, 3, 1};
  std::int64_t to[4] = {};

  my_memcopy(to, from, 4);

  return 0;
}

// clang-format off

// CHECK: STATIC INSTRUCTIONS
// CHECK: ===================

// CHECK:      Address:        [[EXE_PATH]]::.dummy_text::my_memcopy+0x10 (0x[[#%x,READ_ADDR:]])
// CHECK-NEXT: Disassembly:    mov rdx, qword ptr [rsi+rax*1]
// NOLIM-NEXT: Read values:    [01 00 00 00 00 00 00 00, 02 00 00 00 00 00 00 00, 03 00 00 00 00 00 00 00]
// LIMIT-NEXT: Read values:    [01 00 00 00 00 00 00 00, 02 00 00 00 00 00 00 00]
// CHECK-NEXT: Written values: []
// CHECK: Read values entropy:
// CHECK-SAME: 0.091383
// CHECK: Written values entropy:
// CHECK-SAME: 0{{$}}
// CHECK: Number of bytes read:
// CHECK-SAME: 32{{$}}
// CHECK: Number of bytes written:
// CHECK-SAME: 0{{$}}

// CHECK:      Address:        [[EXE_PATH]]::.dummy_text::my_memcopy+0x14 (0x[[#%x,WRITE_ADDR:]])
// CHECK-NEXT: Disassembly:    mov qword ptr [rdi+rax*1], rdx
// CHECK-NEXT: Read values:    []
// NOLIM-NEXT: Written values: [01 00 00 00 00 00 00 00, 02 00 00 00 00 00 00 00, 03 00 00 00 00 00 00 00]
// LIMIT-NEXT: Written values: [01 00 00 00 00 00 00 00, 02 00 00 00 00 00 00 00]
// CHECK: Read values entropy:
// CHECK-SAME: 0{{$}}
// CHECK: Written values entropy:
// CHECK-SAME: 0.091383
// CHECK: Number of bytes read:
// CHECK-SAME: 0{{$}}
// CHECK: Number of bytes written:
// CHECK-SAME: 32{{$}}

// clang-format on
