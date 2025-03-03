/*
RUN: cat dav1d_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/

#include "dav1d.h"
#include <ia2_test_runner.h>

#define IA2_COMPARTMENT 2
#include <ia2_compartment_init.inc>

// LINKARGS: --wrap=dav1d_open
int dav1d_open(Dav1dContext **const c_out, const Dav1dSettings *const s) {
  return 0;
}

// LINKARGS: --wrap=dav1d_close
void dav1d_close(Dav1dContext **const c_out) {
  return;
}

// LINKARGS: --wrap=dav1d_get_picture
int dav1d_get_picture(Dav1dContext *const c, Dav1dPicture *const out) {
  return 0;
}

void dav1d_get_picture_post_condition(Dav1dContext *const c, Dav1dPicture *const out) {
  return;
}
