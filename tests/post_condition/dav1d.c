/*
RUN: cat dav1d_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/

#include <ia2_test_runner.h>
#include "dav1d.h"

// LINKARGS: --wrap=dav1d_get_picture
int dav1d_get_picture(Dav1dContext *const c, Dav1dPicture *const out) {
    out->stride[0] = -1;
    return 0;
}

void dav1d_get_picture_post_condition() {
    cr_log_info("dav1d_get_picture post condition ran");
}
