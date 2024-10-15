// RUN: g++ %s -o %t.exe
// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -output %t.log -entropy_sample_interval 1 -replay -replay:basename %t/pinball -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t.log --log_file=%t.log | FileCheck %s

int main(int argc, char *argv[]) {
  // CHECK:      {{[^[:space:]]+}}/libc.so.6::malloc(size = 1)
  // CHECK-NEXT: ---> address = 0x[[#%x,BUF1:]]
  unsigned char *buf1 = new unsigned char[1];

  // Each bit is sampled 4 times, either:
  // - 3x 0 and 1x 1
  // - 3x 1 and 1x 0
  // --> average temporal entropy = h(1/4)

  buf1[0] = 0b11000110;
  buf1[0] = 0b10101010;
  buf1[0] = 0b00100111;
  buf1[0] = 0b10110100;

  delete[] buf1;

  return 0;
}

// CHECK: MEMORY BUFFERS
// CHECK: ==============

// CHECK:      Buffer:                   Buffer 0x[[#BUF1]] --> 0x[[#BUF1 + 1]]
// CHECK-NEXT: Number of reads:          0
// CHECK-NEXT: Number of writes:         4
// CHECK: Average temporal entropy (bit_shannon):
// CHECK-SAME: 0.811278
