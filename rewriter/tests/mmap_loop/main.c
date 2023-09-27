/*
RUN: sh -c 'if [ ! -s "mmap_loop_call_gates_0.ld" ]; then echo "No link args as expected"; exit 0; fi; echo "Unexpected link args"; exit 1;'
*/
#include "mmap_loop.h"
#include <assert.h>
#include <ia2.h>
#include <math.h>
#include <stdio.h>
#include <criterion/criterion.h>

#define IA2_DEFINE_TEST_HANDLER
#include "test_fault_handler.h"

/*
    This program tests that mmap and heap allocations are handled properly.
*/

INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

Test(mmap_loop, main) {
  cr_log_info("program started");

  char *buf = malloc(4096);
  buf[0] = 'r';
  assert(buf[0] == 'r');

  char *lib_buf_malloc = lib_malloc_buf(4096);

  cr_log_info("mmap buf");
  char *lib_buf_mmap = lib_mmap_buf(4096);

  free(buf);

  // syscall tracing should forbid us to unmap compartment 2's buffer
  // this requires the track-memory-map runtime which isn't enabled in tests
  int res = munmap(lib_buf_mmap, 4096);
  if (res < 0) {
    perror("munmap");
    cr_log_info("Was not able to unmap memory in another compartment (as expected)");
  } else {
    cr_log_info("Able to unmap other compartment's memory (runtime not enabled)");
  }
}