// RUN: gcc -g %s -o %t.exe -masm=intel

// RUN: %sde -log -log:basename %t/pinball -- %t.exe
// RUN: %sde %toolarg -csv_prefix %t -replay -replay:basename %t/pinball -replay:addr_trans -- nullapp
// RUN: patch-debug-info.py <%t.call-targets.csv >%t.call-targets.patched.csv
// RUN: mv %t.call-targets.patched.csv %t.call-targets.csv
// RUN: pretty-print-csvs.py --prefix=%t > %t.out

// RUN: FileCheck %s <%t.out -DFILE=%s

void f(int);
void g(int);
void h(int);
void i(int);

void f(int n) {
  if (n == 0)
    return;

  // CHECK: Call:       [[FILE]]:[[#@LINE+2]]:[[#]]
  // CHECK: Callee 1/1: g ([[FILE]]:[[#G_LINE:]]:[[#]])
  g(n - 1);

  // CHECK: Call:       [[FILE]]:[[#@LINE+2]]:[[#]]
  // CHECK: Callee 1/1: g ([[FILE]]:[[#G_LINE]]:[[#]])
  g(n - 1);
}

void g(int n) {
  if (n == 0)
    return;

  // CHECK: Call:       [[FILE]]:[[#@LINE+2]]:[[#]]
  // CHECK: Callee 1/1: h ([[FILE]]:[[#H_LINE:]]:[[#]])
  h(n - 1);
}

void h(int n) {
  if (n == 0)
    return;

  // CHECK: Call:       [[FILE]]:[[#@LINE+2]]:[[#]]
  // CHECK: Callee 1/1: i ([[FILE]]:[[#I_LINE:]]:[[#]])
  i(n - 1);

  // CHECK: Call:       [[FILE]]:[[#@LINE+2]]:[[#]]
  // CHECK: Callee 1/1: f ([[FILE]]:[[#F_LINE:]]:[[#]])
  f(n - 1);
}

void i(int n) {
  if (n == 0)
    return;

  // CHECK: Call:       [[FILE]]:[[#@LINE+2]]:[[#]]
  // CHECK: Callee 1/1: g ([[FILE]]:[[#G_LINE]]:[[#]])
  g(n - 1);

  // CHECK: Call:       [[FILE]]:[[#@LINE+2]]:[[#]]
  // CHECK: Callee 1/1: i ([[FILE]]:[[#I_LINE]]:[[#]])
  i(n - 1);
}

int main() { f(5); }
