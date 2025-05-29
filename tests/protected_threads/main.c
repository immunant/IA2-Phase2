/*
RUN: sh -c 'if [ ! -s "protected_threads_call_gates_0.ld" ]; then echo "No link args as expected"; exit 0; fi; echo "Unexpected link args"; exit 1;'
*/
#include "library.h"
#include <assert.h>
#include <ia2.h>
#include <pthread.h>
#include <string.h>
#include <sys/wait.h>

#include <ia2_test_runner.h>

#include <unistd.h>

INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

void ia2_main(void) {
    ia2_register_compartment("main", 1, NULL);
    ia2_register_compartment("libprotected_threads_lib.so", 2, NULL);
}

void *nop(void *unused) { return NULL; }

Test(protected_threads, main) {
  pthread_t t;
#if IA2_ENABLE
  int ret = pthread_create(&t, NULL, nop, NULL);
  cr_assert(!ret);
#endif
  pthread_join(t, NULL);
}
