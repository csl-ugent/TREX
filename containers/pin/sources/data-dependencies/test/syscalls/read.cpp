// RUN: g++ %s -o %t.exe -masm=intel

// RUN: nm --numeric-sort %t.exe > %t.symbols

// RUN: %sde -log -log:basename %t/pinball -- %t.exe %S/Input/test.txt key!
// RUN: %sde %toolarg -csv_prefix %t -syscall_file %S/Input/syscalls.csv -replay -replay:basename %t/pinball -replay:addr_trans -replay:playout -- nullapp >%t.appout
// RUN: pretty-print-csvs.py --prefix=%t > %t.out

// RUN: cat %t.appout %t.symbols %t.out | tee /tmp/out | \
// RUN: FileCheck %s -DEXE_NAME=%basename_t.tmp.exe

// REQUIRES: x64

#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>

asm(R"(
    .section    .text
    .globl      encrypt

.balign 16; encrypt:

.balign 16; .copyloop:
.balign 16;     mov         al, BYTE PTR [rsi]
.balign 16;     mov         ah, BYTE PTR [rdx]
.balign 16;     xor         ah, al
.balign 16;     mov         BYTE PTR [rdi], ah
.balign 16;     inc         rsi
.balign 16;     inc         rdx
.balign 16;     inc         rdi
.balign 16;     loop        .copyloop

.balign 16;     ret
)");

extern "C" void encrypt(char *out, char *data, char *key, std::int64_t len);

int main(int argc, char *argv[]) {
  char *inputpath = argv[1];
  char *key = argv[2];

  char data[5];
  memset(data, 0, 5);

  int fd = open(inputpath, O_RDONLY);
  read(fd, data, 4);
  close(fd);

  char out[5];
  memset(out, 0, 5);

  encrypt(out, data, key, 4);

  std::cout << "Address of data: " << static_cast<void *>(&data[0])
            << std::endl;
  std::cout << "Address of key: " << static_cast<void *>(&key[0]) << std::endl;
  std::cout << "Address of out: " << static_cast<void *>(&out[0]) << std::endl;

  std::cout << "Output: ";

  for (int i = 0; i < 4; ++i) {
    std::cout << std::showbase << std::hex << (int)out[i] << " ";
  }
  std::cout << "\n";

  return 0;
}

// clang-format off

// Grab the addresses of the buffers.

// CHECK: Address of data: 0x[[#%x,DATA_ADDR:]]
// CHECK: Address of key: 0x[[#%x,KEY_ADDR:]]
// CHECK: Address of out: 0x[[#%x,OUT_ADDR:]]

// Grab the start address of the 'encrypt' function.
// CHECK: [[#%x,ENCRYPT_ADDR:]] {{.*}} encrypt

// CHECK: SYSTEM CALL INSTRUCTIONS
// CHECK: ========================

// CHECK: Instruction: [[SYSCALL_IMG:[^+]*]]+0x[[#%x,SYSCALL_OFFSET:]]

// CHECK: MEMORY DEPENDENCIES
// CHECK: ===================

// CHECK:      Instructions: [[SYSCALL_IMG]]+0x[[#SYSCALL_OFFSET]] <- [[EXE_NAME]]+0x[[#ENCRYPT_ADDR + mul(0, 16)]]
// CHECK:      Memory address:
// CHECK-SAME: 0x[[#DATA_ADDR]]

// CHECK:      Instructions: [[SYSCALL_IMG]]+0x[[#SYSCALL_OFFSET]] <- [[EXE_NAME]]+0x[[#ENCRYPT_ADDR + mul(0, 16)]]
// CHECK:      Memory address:
// CHECK-SAME: 0x[[#DATA_ADDR + 1]]

// CHECK:      Instructions: [[SYSCALL_IMG]]+0x[[#SYSCALL_OFFSET]] <- [[EXE_NAME]]+0x[[#ENCRYPT_ADDR + mul(0, 16)]]
// CHECK:      Memory address:
// CHECK-SAME: 0x[[#DATA_ADDR + 2]]

// CHECK:      Instructions: [[SYSCALL_IMG]]+0x[[#SYSCALL_OFFSET]] <- [[EXE_NAME]]+0x[[#ENCRYPT_ADDR + mul(0, 16)]]
// CHECK:      Memory address:
// CHECK-SAME: 0x[[#DATA_ADDR + 3]]
