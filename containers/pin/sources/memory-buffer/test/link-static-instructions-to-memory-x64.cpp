// RUN: g++ %s -o %t.exe -masm=intel
// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -output %t.log -replay -replay:basename %t/pinball -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t.log --log_file=%t.log | FileCheck %s -DEXE_PATH=%t.exe

// XFAIL: x86

#include <cstdint>

// Test is based on test/memory-regions.cpp and test/static-instructions.cpp.

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

// Helper function to align a pointer.
std::int64_t *align_ptr(unsigned char *ptr) {
  auto ptr_num = reinterpret_cast<std::uintptr_t>(ptr);
  std::int64_t *ptr_aligned =
      reinterpret_cast<std::int64_t *>(ptr_num + 16 - (ptr_num % 16));

  return ptr_aligned;
}

int main(int argc, char *argv[]) {
  // CHECK:      {{[^[:space:]]+}}/libc.so.6::malloc(size = 80)
  // CHECK-NEXT: ---> address = 0x[[#%x,BUF_FROM:]]

  // CHECK:      {{[^[:space:]]+}}/libc.so.6::malloc(size = 80)
  // CHECK-NEXT: ---> address = 0x[[#%x,BUF_INTERM:]]

  // CHECK:      {{[^[:space:]]+}}/libc.so.6::malloc(size = 80)
  // CHECK-NEXT: ---> address = 0x[[#%x,BUF_TO:]]

  // Allocate 64 bytes, plus 16 bytes extra for alignment.
  // We are going to use these as aligned buffers of size 64 bytes.
  unsigned char *from = new unsigned char[64 + 16];
  unsigned char *intermediate = new unsigned char[64 + 16];
  unsigned char *to = new unsigned char[64 + 16];

  // Align arrays on 16 bytes. Note that we use both pointers as pointers to an
  // array of 8 64-bit integers.
  std::int64_t *from_aligned = align_ptr(from);
  std::int64_t *intermediate_aligned = align_ptr(intermediate);
  std::int64_t *to_aligned = align_ptr(to);

  // Call our memcopy function on these aligned buffers.
  my_memcopy(intermediate_aligned, from_aligned, 8);
  my_memcopy(to_aligned, intermediate_aligned, 8);

  // CHECK: {{[^[:space:]]+}}/libc.so.6::free(ptr = 0x[[#BUF_FROM]])
  delete[] from;

  // CHECK: {{[^[:space:]]+}}/libc.so.6::free(ptr = 0x[[#BUF_INTERM]])
  delete[] intermediate;

  // CHECK: {{[^[:space:]]+}}/libc.so.6::free(ptr = 0x[[#BUF_TO]])
  delete[] to;

  return 0;
}

// clang-format off

// Capture the RIP of interesting instructions.

// CHECK: STATIC INSTRUCTIONS
// CHECK: ===================

// CHECK: Address: [[EXE_PATH]]::.dummy_text::my_memcopy+0x10 (0x[[#%x,READ_ADDR:]])
// CHECK: Address: [[EXE_PATH]]::.dummy_text::my_memcopy+0x14 (0x[[#%x,WRITE_ADDR:]])

// CHECK: MEMORY BUFFERS
// CHECK: ==============

// CHECK: Buffer: Buffer 0x[[#BUF_FROM]] --> 0x[[#BUF_FROM + 80]]
// CHECK: Read instructions:
// CHECK-SAME: [0x[[#READ_ADDR]]]
// CHECK: Write instructions:
// CHECK-SAME: []

// CHECK: Buffer: Buffer 0x[[#BUF_INTERM]] --> 0x[[#BUF_INTERM + 80]]
// CHECK: Read instructions:
// CHECK-SAME: [0x[[#READ_ADDR]]]
// CHECK: Write instructions:
// CHECK-SAME: [0x[[#WRITE_ADDR]]]

// CHECK: Buffer: Buffer 0x[[#BUF_TO]] --> 0x[[#BUF_TO + 80]]
// CHECK: Read instructions:
// CHECK-SAME: []
// CHECK: Write instructions:
// CHECK-SAME: [0x[[#WRITE_ADDR]]]

// CHECK: MEMORY REGIONS
// CHECK: ==============

// 4 regions of 16 bytes for 'from'.
// ---------------------------------

// CHECK: Buffer: Buffer 0x[[#mul(div(BUF_FROM, 16) + 1, 16)]] --> 0x[[#mul(div(BUF_FROM, 16) + 2, 16)]]
// CHECK: Read instructions:
// CHECK-SAME: [0x[[#READ_ADDR]]]
// CHECK: Write instructions:
// CHECK-SAME: []

// CHECK: Buffer: Buffer 0x[[#mul(div(BUF_FROM, 16) + 2, 16)]] --> 0x[[#mul(div(BUF_FROM, 16) + 3, 16)]]
// CHECK: Read instructions:
// CHECK-SAME: [0x[[#READ_ADDR]]]
// CHECK: Write instructions:
// CHECK-SAME: []

// CHECK: Buffer: Buffer 0x[[#mul(div(BUF_FROM, 16) + 3, 16)]] --> 0x[[#mul(div(BUF_FROM, 16) + 4, 16)]]
// CHECK: Read instructions:
// CHECK-SAME: [0x[[#READ_ADDR]]]
// CHECK: Write instructions:
// CHECK-SAME: []

// CHECK: Buffer: Buffer 0x[[#mul(div(BUF_FROM, 16) + 4, 16)]] --> 0x[[#mul(div(BUF_FROM, 16) + 5, 16)]]
// CHECK: Read instructions:
// CHECK-SAME: [0x[[#READ_ADDR]]]
// CHECK: Write instructions:
// CHECK-SAME: []

// 4 regions of 16 bytes for 'intermediate'.
// -----------------------------------------

// CHECK: Buffer: Buffer 0x[[#mul(div(BUF_INTERM, 16) + 1, 16)]] --> 0x[[#mul(div(BUF_INTERM, 16) + 2, 16)]]
// CHECK: Read instructions:
// CHECK-SAME: [0x[[#READ_ADDR]]]
// CHECK: Write instructions:
// CHECK-SAME: [0x[[#WRITE_ADDR]]]

// CHECK: Buffer: Buffer 0x[[#mul(div(BUF_INTERM, 16) + 2, 16)]] --> 0x[[#mul(div(BUF_INTERM, 16) + 3, 16)]]
// CHECK: Read instructions:
// CHECK-SAME: [0x[[#READ_ADDR]]]
// CHECK: Write instructions:
// CHECK-SAME: [0x[[#WRITE_ADDR]]]

// CHECK: Buffer: Buffer 0x[[#mul(div(BUF_INTERM, 16) + 3, 16)]] --> 0x[[#mul(div(BUF_INTERM, 16) + 4, 16)]]
// CHECK: Read instructions:
// CHECK-SAME: [0x[[#READ_ADDR]]]
// CHECK: Write instructions:
// CHECK-SAME: [0x[[#WRITE_ADDR]]]

// CHECK: Buffer: Buffer 0x[[#mul(div(BUF_INTERM, 16) + 4, 16)]] --> 0x[[#mul(div(BUF_INTERM, 16) + 5, 16)]]
// CHECK: Read instructions:
// CHECK-SAME: [0x[[#READ_ADDR]]]
// CHECK: Write instructions:
// CHECK-SAME: [0x[[#WRITE_ADDR]]]

// 4 regions of 16 bytes for 'to'.
// -------------------------------

// CHECK: Buffer: Buffer 0x[[#mul(div(BUF_TO, 16) + 1, 16)]] --> 0x[[#mul(div(BUF_TO, 16) + 2, 16)]]
// CHECK: Read instructions:
// CHECK-SAME: []
// CHECK: Write instructions:
// CHECK-SAME: [0x[[#WRITE_ADDR]]]

// CHECK: Buffer: Buffer 0x[[#mul(div(BUF_TO, 16) + 2, 16)]] --> 0x[[#mul(div(BUF_TO, 16) + 3, 16)]]
// CHECK: Read instructions:
// CHECK-SAME: []
// CHECK: Write instructions:
// CHECK-SAME: [0x[[#WRITE_ADDR]]]

// CHECK: Buffer: Buffer 0x[[#mul(div(BUF_TO, 16) + 3, 16)]] --> 0x[[#mul(div(BUF_TO, 16) + 4, 16)]]
// CHECK: Read instructions:
// CHECK-SAME: []
// CHECK: Write instructions:
// CHECK-SAME: [0x[[#WRITE_ADDR]]]

// CHECK: Buffer: Buffer 0x[[#mul(div(BUF_TO, 16) + 4, 16)]] --> 0x[[#mul(div(BUF_TO, 16) + 5, 16)]]
// CHECK: Read instructions:
// CHECK-SAME: []
// CHECK: Write instructions:
// CHECK-SAME: [0x[[#WRITE_ADDR]]]

// clang-format on
