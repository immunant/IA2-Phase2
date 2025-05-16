/*
RUN: cat threads_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/
#include <ia2_test_runner.h>

#include "library.h"
#include <ia2.h>
#include <stdio.h>
#include <stdlib.h>

int data_in_lib = 900;

// LINKARGS: --wrap=library_access_int_ptr
int library_access_int_ptr(int *ptr) { return *ptr; }

// LINKARGS: --wrap=library_call_fn
void library_call_fn(Fn what) {
  cr_log_info("in lib, about to call fnptr; lib data: %d\n", data_in_lib);
  cr_assert_eq(data_in_lib, 900);
  what();
}

// LINKARGS: --wrap=library_foo
void library_foo() {
  cr_log_info("data in library: %d\n", data_in_lib);
  cr_assert_eq(data_in_lib, 900);
}

// LINKARGS: --wrap=library_memset
void library_memset(void *ptr, uint8_t byte, size_t n) {
  char *char_ptr = (char *)ptr;
  for (size_t i = 0; i < n; i++) {
    char_ptr[i] = byte;
  }
}

// LINKARGS: --wrap=library_showpkru
void library_showpkru() {
  uint32_t actual_tag = ia2_get_tag();
  cr_log_info("library tag %08x", actual_tag);
#if defined(__aarch64__)
  cr_assert_eq(0, actual_tag);
#else
  cr_assert_eq(0xfffffffc, actual_tag);
#endif
}

static void *library_showpkru_thread_main(void *unused) {
  library_showpkru();
  return NULL;
}

// LINKARGS: --wrap=library_spawn_thread
pthread_t library_spawn_thread(void) {
  pthread_t thread;
  int ret = pthread_create(&thread, NULL, library_showpkru_thread_main, NULL);
  if (ret < 0) {
    perror("pthread_create");
    exit(1);
  }
  return thread;
}
