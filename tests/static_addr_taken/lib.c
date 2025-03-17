#include <ia2.h>
#include <ia2_test_runner.h>

#define IA2_COMPARTMENT 2
#include <ia2_compartment_init.inc>

#include "static_fns.h"

static void duplicate_noop(void) {
  printf("called %s in library\n", __func__);
}

static void identical_name(void) {
  static int x = 4;
  printf("%s in library read x = %d\n", __func__, x);
}

static fn_ptr_ty ptrs[3] IA2_SHARED_DATA = {
    inline_noop, duplicate_noop, identical_name};

fn_ptr_ty *get_ptrs_in_lib(void) {
  return ptrs;
}
