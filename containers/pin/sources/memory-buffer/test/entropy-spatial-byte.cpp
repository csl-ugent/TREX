// RUN: g++ %s -o %t.exe
// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -output %t.log -entropy_sample_interval 16 -replay -replay:basename %t/pinball -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t.log --log_file=%t.log | FileCheck %s

#include <algorithm>
#include <array>
#include <new>

int main(int argc, char *argv[]) {
  // CHECK:      {{[^[:space:]]+}}/libc.so.6::malloc(size = 8)
  // CHECK-NEXT: ---> address = 0x[[#%x,BUF1:]]
  unsigned char *buf1 = new unsigned char[8];

  // CHECK:      {{[^[:space:]]+}}/libc.so.6::malloc(size = 8)
  // CHECK-NEXT: ---> address = 0x[[#%x,BUF2:]]
  unsigned char *buf2 = new unsigned char[8];

  // CHECK:      {{[^[:space:]]+}}/libc.so.6::malloc(size = 8)
  // CHECK-NEXT: ---> address = 0x[[#%x,BUF3:]]
  unsigned char *buf3 = new unsigned char[8];

  // buf1 and buf2 will be filled with the bytes in data1/data2 in some order,
  // respectively. buf3 will be filled 1/3 of the time with data1, and 2/3 with
  // data2.
  unsigned char data1[] = {10, 20, 20, 20, 30, 30, 40, 50};
  unsigned char data2[] = {50, 50, 50, 50, 75, 75, 100, 200};

  // We thus have the following byte-level entropy statistics:

  // byte_shannon
  // ============
  // buf1: H({1/8, 3/8, 2/8, 1/8, 1/8, 0, ...}) / log2(256) = 0.26945
  // buf2: H({4/8, 2/8, 1/8, 1/8, 0, ...}) / log2(256) = 0.21875
  // buf3: 1/3 * buf1 + 2/3 * buf2 = 0.23565

  // byte_shannon_adapted
  // ====================
  // buf1: H({1/8, 3/8, 2/8, 1/8, 1/8, 0, ...}) / log2(8) = 0.71854
  // buf2: H({4/8, 2/8, 1/8, 1/8, 0, ...}) / log2(8) = 0.58333
  // buf3: 1/3 * buf1 + 2/3 * buf2 = 0.62840

  // byte_num_different
  // ==================
  // buf1: 5 (10, 20, 30, 40, 50)
  // buf2: 4 (50, 75, 100, 200)
  // buf3: 1/3 * buf1 + 2/3 * buf2 = 4.33333

  // byte_num_unique
  // ===============
  // buf1: 3 (10, 40, 50)
  // buf2: 2 (100, 200)
  // buf3: 1/3 * buf1 + 2/3 * buf2 = 2.33333

  // byte_average
  // ============
  // buf1: (10+3*20+2*30+40+50)/8 = 27.5
  // buf2: (4*50+2*75+100+200)/8 = 81.25
  // buf3: 1/3 * buf1 + 2/3 * buf2 = 63.33333

  // Do this for 96 writes = 6 sample intervals, shuffling after every 8 writes.
  for (std::size_t i = 0; i < 96; ++i) {
    if (i % 8 == 0) {
      std::random_shuffle(std::begin(data1), std::end(data1));
      std::random_shuffle(std::begin(data2), std::end(data2));
    }

    buf1[i % 8] = data1[i % 8];
    buf2[i % 8] = data2[i % 8];

    // i = 0 -> 31: buf1 (1/3 of the time)
    // i = 32 -> 96: buf2 (2/3 of the time)
    buf3[i % 8] = (i < 32 ? data1 : data2)[i % 8];
  }

  delete[] buf1;
  delete[] buf2;
  delete[] buf3;

  return 0;
}

// clang-format off

// BUFFER 1
// ========

// CHECK:      Buffer:          Buffer 0x[[#BUF1]] --> 0x[[#BUF1 + 8]]
// CHECK:      Number of reads:
// CHECK-SAME: 0
// CHECK:      Number of writes:
// CHECK-SAME: 96
// CHECK:      Average spatial entropy (byte_shannon):
// CHECK-SAME: 0.26945
// CHECK:      Average spatial entropy (byte_shannon_adapted):
// CHECK-SAME: 0.71854
// CHECK:      Average spatial entropy (byte_num_different):
// CHECK-SAME: 5
// CHECK:      Average spatial entropy (byte_num_unique):
// CHECK-SAME: 3
// CHECK:      Average spatial entropy (byte_average):
// CHECK-SAME: 27.5

// BUFFER 2
// ========

// CHECK:      Buffer:          Buffer 0x[[#BUF2]] --> 0x[[#BUF2 + 8]]
// CHECK:      Number of reads:
// CHECK-SAME: 0
// CHECK:      Number of writes:
// CHECK-SAME: 96
// CHECK:      Average spatial entropy (byte_shannon):
// CHECK-SAME: 0.21875
// CHECK:      Average spatial entropy (byte_shannon_adapted):
// CHECK-SAME: 0.58333
// CHECK:      Average spatial entropy (byte_num_different):
// CHECK-SAME: 4
// CHECK:      Average spatial entropy (byte_num_unique):
// CHECK-SAME: 2
// CHECK:      Average spatial entropy (byte_average):
// CHECK-SAME: 81.25

// BUFFER 3
// ========

// CHECK:      Buffer:          Buffer 0x[[#BUF3]] --> 0x[[#BUF3 + 8]]
// CHECK:      Number of reads:
// CHECK-SAME: 0
// CHECK:      Number of writes:
// CHECK-SAME: 96
// CHECK:      Average spatial entropy (byte_shannon):
// CHECK-SAME: 0.23565
// CHECK:      Average spatial entropy (byte_shannon_adapted):
// CHECK-SAME: 0.62840
// CHECK:      Average spatial entropy (byte_num_different):
// CHECK-SAME: 4.33333
// CHECK:      Average spatial entropy (byte_num_unique):
// CHECK-SAME: 2.33333
// CHECK:      Average spatial entropy (byte_average):
// CHECK-SAME: 63.3333

// clang-format on
