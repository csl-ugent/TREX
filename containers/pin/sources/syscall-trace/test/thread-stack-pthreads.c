// RUN: gcc %s -o %t.exe

// RUN: %sde -log -log:basename %t/pinball -log:mt -- %t.exe >%t.appout
// RUN: %sde %toolarg -csv_prefix %t -replay -replay:basename %t/pinball \
// RUN: -replay:addr_trans -- nullapp
// RUN: pretty-print-csvs.py --prefix=%t | \
// RUN: FileCheck %s

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

static const unsigned int NUM_THREADS = 100;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void threadfunc(void *) {
  int stack[24];

  pthread_mutex_lock(&mutex);
  printf("stack: %p\n", &stack[0]);
  pthread_mutex_unlock(&mutex);
}

int main() {
  pthread_t threads[NUM_THREADS];

  // CHECK-COUNT-100: mmap{{2?}}(0x0, [[#]], PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_STACK, -1, 0) = 0x[[#%x,]]
  // CHECK-NOT: MAP_STACK

  for (unsigned int i = 0; i < NUM_THREADS; ++i) {
    threads[i] = pthread_create(&threads[i], NULL, threadfunc, NULL);
  }

  for (unsigned int i = 0; i < NUM_THREADS; ++i) {
    pthread_join(threads[i], NULL);
  }

  return 0;
}
