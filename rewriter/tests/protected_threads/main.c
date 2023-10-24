/*
*/
#include "library.h"
#include <assert.h>
#include <ia2.h>
#include <pthread.h>
#include <string.h>
#include <sys/wait.h>
#define IA2_DEFINE_TEST_HANDLER
#include <criterion/criterion.h>
#include <test_fault_handler.h>
#include <unistd.h>

INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

void *nop(void *unused) { return NULL; }

Test(protected_threads, main) {
  pthread_t t;
#if IA2_ENABLE
  int ret = pthread_create(&t, NULL, nop, NULL);
  cr_assert(!ret);
#endif
  pthread_join(t, NULL);
}
