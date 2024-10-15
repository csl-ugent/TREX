#include <cstdint>

#ifdef PIN_TARGET_ARCH_X64

// Int with the size of a machine register.
typedef std::int64_t native_int;

// Copies 'len' elements from array 'from' to array 'to'.
// to:   rdi
// from: rsi
// len:  rdx
extern "C" void my_memcopy(native_int *to, native_int *from, native_int len);

asm(R"(
  .section        .text
  .intel_syntax   noprefix
  .globl          my_memcopy
  .type           my_memcopy, @function

my_memcopy:
  test            rdx, rdx
  jle             .end
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
)");

#else

// Int with the size of a machine register.
typedef std::int32_t native_int;

// Copies 'len' elements from array 'from' to array 'to'.
// to:   [esp + 8]
// from: [esp + 12]
// len:  [esp + 16]
extern "C" void my_memcopy(native_int *to, native_int *from, native_int len);

asm(R"(
  .section        .text
  .intel_syntax   noprefix
  .globl          my_memcopy
  .type           my_memcopy, @function

my_memcopy:
  push            ebx
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
  ret
)");

#endif
