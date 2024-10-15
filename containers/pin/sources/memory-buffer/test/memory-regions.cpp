// RUN: g++ %s -o %t.exe
// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -output %t.log -entropy_sample_interval 16 -replay -replay:basename %t/pinball -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t.log --log_file=%t.log | FileCheck %s

#include <cstdint>
#include <memory>
#include <new>

int main(int argc, char *argv[]) {
  // CHECK:      {{[^[:space:]]+}}/libc.so.6::malloc(size = 80)
  // CHECK-NEXT: ---> address = 0x[[#%x,BUF:]]

  // Allocate 64 bytes, plus 16 bytes extra for alignment.
  // We are going to use this as an aligned buffer of size 64 bytes.
  unsigned char *p = new unsigned char[64 + 16];

  // Align p on 16 bytes.
  auto p_num = reinterpret_cast<std::uintptr_t>(p);
  unsigned char *p_aligned =
      reinterpret_cast<unsigned char *>(p_num + 16 - (p_num % 16));

  // Fill the entire 64 byte buffer 4 times.
  // The values are of the form 0btt00xxxx.
  // where tt = 00, 01, 10, 11 at different time instants
  // where xxxx = 0000, ..., 1111 at different locations within a region

  // Spatial entropy:
  // (1) sum of hamming weight of xxxx: 0000, ..., 1111 = 1 * 0 + 4 * 1 + 6 * 2
  // + 4 * 3 + 1 * 4 = 32.
  // (2) this is increased by 16*0 (00), 16*1 (01), 16*1 (10), 16*2 (11) for the
  // tt bits.
  // (3) so spatial entropy is 1/4 * (h(32/128) + 2 * h(48/128) +
  // h(64/128))

  // Temporal entropy:
  // (1) For each 8-bit value:
  //     - 2 bits (tt) are equiprobably 0/1  --> h(2/4) = 1
  //     - 2 bits (00) are always 0          --> h(0/4) = 0
  //     - 4 bits (xxxx) are always the same --> h(0/4) or h(4/4) = 0
  // (2) so temporal entropy is 2/8 * h(2/4) = 0.25

  for (int round = 0; round < 4; ++round) {
    for (int i = 0; i < 64; ++i) {
      p_aligned[i] = (round << 6) | (i % 16);
    }
  }

  // CHECK: {{[^[:space:]]+}}/libc.so.6::free(ptr = 0x[[#BUF]])
  delete[] p;

  return 0;
}

// CHECK: MEMORY REGIONS
// CHECK: ==============

// clang-format off

// CHECK:      Buffer: Buffer 0x[[#mul(div(BUF, 16) + 1, 16)]] --> 0x[[#mul(div(BUF, 16) + 2, 16)]]
// CHECK:      Number of reads:
// CHECK-SAME: 0
// CHECK:      Number of writes:
// CHECK-SAME: 64
// CHECK:      Average spatial entropy (bit_shannon):
// CHECK-SAME: 0.930037
// CHECK:      Average temporal entropy (bit_shannon):
// CHECK-SAME: 0.25

// CHECK:      Buffer: Buffer 0x[[#mul(div(BUF, 16) + 2, 16)]] --> 0x[[#mul(div(BUF, 16) + 3, 16)]]
// CHECK:      Number of reads:
// CHECK-SAME: 0
// CHECK:      Number of writes:
// CHECK-SAME: 64
// CHECK:      Average spatial entropy (bit_shannon):
// CHECK-SAME: 0.930037
// CHECK:      Average temporal entropy (bit_shannon):
// CHECK-SAME: 0.25

// CHECK:      Buffer: Buffer 0x[[#mul(div(BUF, 16) + 3, 16)]] --> 0x[[#mul(div(BUF, 16) + 4, 16)]]
// CHECK:      Number of reads:
// CHECK-SAME: 0
// CHECK:      Number of writes:
// CHECK-SAME: 64
// CHECK:      Average spatial entropy (bit_shannon):
// CHECK-SAME: 0.930037
// CHECK:      Average temporal entropy (bit_shannon):
// CHECK-SAME: 0.25

// CHECK:      Buffer: Buffer 0x[[#mul(div(BUF, 16) + 4, 16)]] --> 0x[[#mul(div(BUF, 16) + 5, 16)]]
// CHECK:      Number of reads:
// CHECK-SAME: 0
// CHECK:      Number of writes:
// CHECK-SAME: 64
// CHECK:      Average spatial entropy (bit_shannon):
// CHECK-SAME: 0.930037
// CHECK:      Average temporal entropy (bit_shannon):
// CHECK-SAME: 0.25

// clang-format on
