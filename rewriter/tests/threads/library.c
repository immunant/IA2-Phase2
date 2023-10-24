/*
*/
#include <criterion/criterion.h>
#include <criterion/logging.h>
#include "library.h"
#include <ia2.h>
#include <stdio.h>
#include <stdlib.h>

int data_in_lib = 900;

int library_access_int_ptr(int *ptr) { return *ptr; }

void library_call_fn(Fn what) {
  cr_log_info("in lib, about to call fnptr; lib data: %d\n", data_in_lib);
  cr_assert_eq(data_in_lib, 900);
  what();
}

void library_foo() {
  cr_log_info("data in library: %d\n", data_in_lib);
  cr_assert_eq(data_in_lib, 900);
}

void library_memset(void *ptr, uint8_t byte, size_t n) {
  char *char_ptr = (char *)ptr;
  for (size_t i = 0; i < n; i++) {
    char_ptr[i] = byte;
  }
}

void library_showpkru() {
  uint32_t actual_pkru = ia2_get_pkru();
  cr_log_info("library pkru %08x", actual_pkru);
  cr_assert_eq(0xfffffffc, actual_pkru);
}

static void *library_showpkru_thread_main(void *unused) {
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
