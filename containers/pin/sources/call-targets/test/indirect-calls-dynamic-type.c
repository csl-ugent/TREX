// RUN: gcc -g %s -o %t.exe -masm=intel

// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -csv_prefix %t -replay -replay:basename %t/pinball -replay:addr_trans -- nullapp
// RUN: patch-debug-info.py <%t.call-targets.csv >%t.call-targets.patched.csv
// RUN: mv %t.call-targets.patched.csv %t.call-targets.csv
// RUN: pretty-print-csvs.py --prefix=%t > %t.out

// RUN: FileCheck %s <%t.out -DFILE=%s

void void_int_1(int x) {}
void void_int_2(int x) {}

void void_float_1(float x) {}

int int_int_1(int x) { return 0; }

void do_void_int_call(void (*void_int)(int)) {
  // CHECK: Call:       [[FILE]]:[[#@LINE+3]]:[[#]]
  // CHECK: Callee 1/2: void_int_1 ([[FILE]]:[[#]]:[[#]])
  // CHECK: Callee 2/2: void_int_2 ([[FILE]]:[[#]]:[[#]])
  void_int(0);
}

void do_void_int_1_call(void (*void_int)(int)) {
  // CHECK: Call:       [[FILE]]:[[#@LINE+2]]:[[#]]
  // CHECK: Callee 1/1: void_int_1 ([[FILE]]:[[#]]:[[#]])
  void_int(0);
}

void do_void_float_call(void (*void_float)(float)) {
  // CHECK: Call:       [[FILE]]:[[#@LINE+2]]:[[#]]
  // CHECK: Callee 1/1: void_float_1 ([[FILE]]:[[#]]:[[#]])
  void_float(0.0);
}

void do_int_int_call(int (*int_int)(int)) {
  // CHECK: Call:       [[FILE]]:[[#@LINE+2]]:[[#]]
  // CHECK: Callee 1/1: int_int_1 ([[FILE]]:[[#]]:[[#]])
  int_int(0);
}

int main() {
  do_void_int_call(&void_int_1);
  do_void_int_call(&void_int_2);

  do_void_int_1_call(&void_int_1);

  do_void_float_call(&void_float_1);

  do_int_int_call(&int_int_1);
}
