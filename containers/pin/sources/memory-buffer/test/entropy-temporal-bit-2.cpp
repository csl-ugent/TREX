// RUN: g++ %s -o %t.exe
// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -output %t.log -entropy_sample_interval 5 -replay -replay:basename %t/pinball -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t.log --log_file=%t.log | FileCheck %s

void do_write(unsigned char *buf, unsigned char value) {
  // Write 5 times (= sample interval).
  for (unsigned int i = 0; i < 5; ++i) {
    buf[0] = value;
  }
}

int main(int argc, char *argv[]) {
  // CHECK:      {{[^[:space:]]+}}/libc.so.6::malloc(size = 1)
  // CHECK-NEXT: ---> address = 0x[[#%x,BUF1:]]
  unsigned char *buf1 = new unsigned char[1];

  // We have the following distribution for the 8 bits in buf1:
  // - 3x always 0 (0)         --> h(0/4) = 0
  // - 2x always 1 (1)         --> h(4/4) = 0
  // - 3x equiprobably 0/1 (p) --> h(2/4) = 1
  // ---> average temporal entropy = h(2/4) * 3/8 = 3/8

  //               0p1p0p10
  do_write(buf1, 0b00110010);
  do_write(buf1, 0b01100010);
  do_write(buf1, 0b01110110);
  do_write(buf1, 0b00100110);

  return 0;
}

// CHECK: MEMORY BUFFERS
// CHECK: ==============

// CHECK:      Buffer:                   Buffer 0x[[#BUF1]] --> 0x[[#BUF1 + 1]]
// CHECK-NEXT: Number of reads:          0
// CHECK-NEXT: Number of writes:         20
// CHECK: Average temporal entropy (bit_shannon):
// CHECK-SAME: 0.375
