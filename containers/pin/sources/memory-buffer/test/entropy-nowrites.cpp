// RUN: g++ %s -o %t.exe
// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -output %t.log -entropy_sample_interval 1 -replay -replay:basename %t/pinball -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t.log --log_file=%t.log | FileCheck %s

int main(int argc, char *argv[]) {
  // CHECK:      {{[^[:space:]]+}}/libc.so.6::malloc(size = 4)
  // CHECK-NEXT: ---> address = 0x[[#%x,BUF1:]]
  unsigned char *buf1 = new unsigned char[4];

  // Buffer that is not written to should display nan.

  delete[] buf1;

  return 0;
}

// clang-format off

// CHECK: MEMORY BUFFERS
// CHECK: ==============

// CHECK:      Buffer:                                Buffer 0x[[#BUF1]] --> 0x[[#BUF1 + 4]]
// CHECK-NEXT: Number of reads:                       0
// CHECK-NEXT: Number of writes:                      0

// CHECK:      Average spatial entropy (bit_shannon):
// CHECK-SAME: nan

// CHECK:      Average spatial entropy (byte_shannon):
// CHECK-SAME: nan

// CHECK:      Average spatial entropy (byte_shannon_adapted):
// CHECK-SAME: nan

// CHECK:      Average spatial entropy (byte_num_different):
// CHECK-SAME: nan

// CHECK:      Average spatial entropy (byte_num_unique):
// CHECK-SAME: nan

// CHECK:      Average spatial entropy (bit_average):
// CHECK-SAME: nan

// CHECK:      Average spatial entropy (byte_average):
// CHECK-SAME: nan

// CHECK:      Average temporal entropy (bit_shannon):
// CHECK-SAME: nan

// clang-format on
