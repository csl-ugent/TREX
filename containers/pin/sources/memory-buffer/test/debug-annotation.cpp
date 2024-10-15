#include <cstdlib>
#include <string>

// clang-format off

// RUN: g++ %s -o %t.exe
// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -output %t.log -replay -replay:basename %t/pinball -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t.log --log_file=%t.log | FileCheck %s -DEXE_PATH=%t.exe -DSRC_PATH=%s


// CHECK: Found MATE_FRAMEWORK_DEBUG_ANNOTATE_MEMORY in image '[[EXE_PATH]]'
extern "C" void MATE_FRAMEWORK_DEBUG_ANNOTATE_MEMORY(const char *address,
                                                     unsigned int size,
                                                     const char *annotation,
                                                     const char *filename,
                                                     unsigned int line) {}

int main(int argc, char *argv[]) {
  // CHECK:      {{[^[:space:]]+}}/libc.so.6::malloc(size = 4)
  // CHECK-NEXT: ---> address = 0x[[#%x,BUF1:]]
  unsigned char *buf1 = new unsigned char[4];

  // CHECK:      {{[^[:space:]]+}}/libc.so.6::malloc(size = 4)
  // CHECK-NEXT: ---> address = 0x[[#%x,BUF2:]]
  unsigned char *buf2 = new unsigned char[4];

  // CHECK:      {{[^[:space:]]+}}/libc.so.6::malloc(size = 4)
  // CHECK-NEXT: ---> address = 0x[[#%x,BUF3:]]
  unsigned char *buf3 = new unsigned char[4];

  // CHECK:      {{[^[:space:]]+}}/libc.so.6::malloc(size = 4)
  // CHECK-NEXT: ---> address = 0x[[#%x,BUF4:]]
  unsigned char *buf4 = new unsigned char[4];

  // Annotate buffers.
  MATE_FRAMEWORK_DEBUG_ANNOTATE_MEMORY((char *)buf1, 4 * sizeof(*buf1), "Memory buffer 1", __FILE__, __LINE__);

  MATE_FRAMEWORK_DEBUG_ANNOTATE_MEMORY((char *)buf2, 4 * sizeof(*buf2), "Memory buffer 2a", __FILE__, __LINE__);
  MATE_FRAMEWORK_DEBUG_ANNOTATE_MEMORY((char *)buf2, 4 * sizeof(*buf2), "Memory buffer 2b", __FILE__, __LINE__);

  for (std::size_t i = 0; i < 4; ++i) {
    std::string annotation = "Memory buffer 3";
    annotation += ('a' + i);

    MATE_FRAMEWORK_DEBUG_ANNOTATE_MEMORY((char *)(buf3 + i), 4 * sizeof(*buf3) - i, annotation.c_str(), __FILE__, __LINE__); }

  // Free buffers.
  delete[] buf1;
  delete[] buf2;
  delete[] buf3;
  delete[] buf4;

  return 0;
}

// CHECK: MEMORY BUFFERS
// CHECK: ==============

// CHECK:      Buffer: Buffer 0x[[#BUF1]] --> 0x[[#BUF1 + 4]]
// CHECK:      Annotation:
// CHECK-SAME: Memory buffer 1(offset=0,size=4,location=[[SRC_PATH]]:37){{$}}

// CHECK:      Buffer: Buffer 0x[[#BUF2]] --> 0x[[#BUF2 + 4]]
// CHECK:      Annotation:
// CHECK-SAME: Memory buffer 2a(offset=0,size=4,location=[[SRC_PATH]]:39);Memory buffer 2b(offset=0,size=4,location=[[SRC_PATH]]:40){{$}}

// CHECK:      Buffer: Buffer 0x[[#BUF3]] --> 0x[[#BUF3 + 4]]
// CHECK:      Annotation:
// CHECK-SAME: Memory buffer 3a(offset=0,size=4,location=[[SRC_PATH]]:46);Memory buffer 3b(offset=1,size=3,location=[[SRC_PATH]]:46);Memory buffer 3c(offset=2,size=2,location=[[SRC_PATH]]:46);Memory buffer 3d(offset=3,size=1,location=[[SRC_PATH]]:46){{$}}

// CHECK:      Buffer: Buffer 0x[[#BUF4]] --> 0x[[#BUF4 + 4]]
// CHECK:      Annotation:
// CHECK-SAME: {{$}}
