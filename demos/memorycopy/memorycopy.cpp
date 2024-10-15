// g++ test.cpp -o test.exe -masm=intel

#include <cstdint>

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
  test            rdx, rdx
  je              .skip_move

  mov             QWORD PTR [rdi+rax], rdx

.skip_move:
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

  delete[] from;
  delete[] intermediate;
  delete[] to;

  return 0;
}
