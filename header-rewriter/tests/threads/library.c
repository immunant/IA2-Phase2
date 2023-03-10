#include "library.h"
#include <ia2.h>
#include <stdio.h>
#include <stdlib.h>

int data_in_lib = 900;

void library_foo() { printf("data in library: %d\n", data_in_lib); }

static inline unsigned int rdpkru(void) {
  uint32_t pkru = 0;
  __asm__ volatile(IA2_RDPKRU : "=a"(pkru) : "a"(0), "d"(0), "c"(0));
  return pkru;
}

void library_showpkru(void) { printf("library pkru %08x\n", rdpkru()); }

void *library_showpkru_thread_main(void *unused) {
  library_showpkru();
  return NULL;
}

pthread_t library_spawn_thread(void) {
  pthread_t thread;
  int ret = pthread_create(&thread, NULL, library_showpkru_thread_main, NULL);
  if (ret < 0) {
    perror("pthread_create");
    exit(1);
  }
  return thread;
}

void library_memset(void *ptr, uint8_t byte, size_t n) {
  char *char_ptr = (char *)ptr;
  for (size_t i = 0; i < n; i++) {
    char_ptr[i] = byte;
  }
}

int library_access_int_ptr(int *ptr) { return *ptr; }

void library_call_fn(Fn what) {
  printf("in lib, about to call fnptr; lib data: %d\n", data_in_lib);
  what();
}
