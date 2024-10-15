// RUN: g++ %s -o %t.exe

// RUN: %sde -log -log:basename %t/pinball -log:mt -- %t.exe >%t.appout
// RUN: %sde %toolarg -csv_prefix %t -replay -replay:basename %t/pinball \
// RUN: -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t | \
// RUN: FileCheck %s

#include <iostream>
#include <mutex>
#include <thread>

static constexpr unsigned int NUM_THREADS = 100;

std::mutex mutex;

void threadfunc() {
  int stack[24];

  std::lock_guard<std::mutex> lock{mutex};
  std::cout << "stack: " << &stack[0] << std::endl;
}

int main() {
  std::thread threads[NUM_THREADS];

  // CHECK-COUNT-100: mmap{{2?}}(0x0, [[#]], PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_STACK, -1, 0) = 0x[[#%x,]]
  // CHECK-NOT: MAP_STACK

  for (unsigned int i = 0; i < NUM_THREADS; ++i) {
    threads[i] = std::thread(threadfunc);
  }

  for (unsigned int i = 0; i < NUM_THREADS; ++i) {
    threads[i].join();
  }

  return 0;
}
