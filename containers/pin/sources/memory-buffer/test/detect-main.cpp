// RUN: g++ %s -o %t.exe
// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -output %t.log -replay -replay:basename %t/pinball -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t.log --log_file=%t.log | FileCheck %s

int main(int argc, char *argv[]) { return 0; }

// Just a sanity check to make sure main() is detected correctly.

// CHECK: __libc_start_main(main = 0x[[#%x,MAIN_ADDRESS:]], ...)
// CHECK: Main called, next ip = 0x[[#%x,END_ADDRESS:]]
// CHECK: Main reached, setting global flag.
// CHECK: Instruction after call to main reached, setting global flag.
