/*
RUN: cat dav1d_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/

#include "dav1d.h"
#include <ia2_test_runner.h>
#include <signal.h>

#define IA2_COMPARTMENT 2
#include <ia2_compartment_init.inc>

bool corrupt_stride IA2_SHARED_DATA = false;

// LINKARGS: --wrap=dav1d_get_picture
int dav1d_get_picture(Dav1dContext *const c, Dav1dPicture *const out) {
  out->stride[0] = 1;
  if (corrupt_stride) {
    out->stride[0] *= -1;
  }
  return 0;
}

IA2_POST_CONDITION(dav1d_get_picture) void dav1d_get_picture_post_condition(Dav1dContext *const c, Dav1dPicture *const out) {
  cr_log_info("dav1d_get_picture post condition ran");
  if (out->stride[0] < 0) {
    cr_log_info("negative stride");
  }
  assert(out->stride[0] > 0);
}
