#include "b63/register.h"
#include <b63/b63.h>
#include <b63/counters/perf_events.h>
#include <ia2.h>

#include "microbench_lib.h"

INIT_RUNTIME(1);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

void ia2_main(void) {
  ia2_register_compartment("main", 1, NULL);
}

B63_BASELINE(no_op, n) {
  for (size_t i = 0; i < n; ++i) {
    no_op_unwrapped();
  }
}

B63_BENCHMARK(no_op_wrapped, n) {
  for (size_t i = 0; i < n; ++i) {
    no_op();
  }
}

int main(int argc, char *argv[]) {
  B63_RUN(argc, argv);
  return 0;
}
