/*
RUN: sh -c 'if [ ! -s "protected_threads_call_gates_0.ld" ]; then echo "No link args as expected"; exit 0; fi; echo "Unexpected link args"; exit 1;'
RUN: %binary_dir/tests/protected_threads/protected_threads_main_wrapped
*/
#include "library.h"
#include <assert.h>
#include <ia2.h>
#include <pthread.h>
#include <string.h>
#include <sys/wait.h>
#define IA2_DEFINE_TEST_HANDLER
#include <test_fault_handler.h>
#include <unistd.h>

INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

void *nop(void *unused) { return NULL; }

int main() {
  pthread_t t;
#if !defined(PRE_REWRITER)
  int ret = pthread_create(&t, NULL, nop, NULL);
#endif
  pthread_join(t, NULL);

  return 0;
}
