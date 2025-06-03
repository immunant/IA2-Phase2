#include "b63/register.h"
#include <b63/b63.h>
#include <b63/counters/perf_events.h>
#include <ia2.h>

#include "microbench_lib.h"
#include "microbench_lib2.h"

INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

void ia2_main(void) {
    ia2_register_compartment("main", 1, NULL);
    ia2_register_compartment("libmicrobenchmark_indirect2.so", 2, NULL);
}

void (*fn_ptr)(void);

IA2_BEGIN_NO_WRAP
void (*fn_ptr_unwrapped)(void);
IA2_END_NO_WRAP

B63_BASELINE(indirect, n) {
  fn_ptr_unwrapped = no_op_unwrapped;
  for (size_t i = 0; i < n; ++i) {
    fn_ptr_unwrapped();
  }
}

B63_BENCHMARK(indirect_wrapped, n) {
  IA2_AS_PTR(fn_ptr) = (void *)no_op_unwrapped;
  for (size_t i = 0; i < n; ++i) {
    fn_ptr();
  }
}

B63_BENCHMARK(indirect_compartment2, n) {
  fn_ptr = no_op_compartment2;
  for (size_t i = 0; i < n; ++i) {
    IA2_CALL(fn_ptr, _ZTSPFvvE);
  }
}

int main(int argc, char *argv[]) {
  B63_RUN(argc, argv);
  return 0;
}
