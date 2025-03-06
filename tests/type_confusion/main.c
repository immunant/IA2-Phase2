/*
RUN: sh -c 'if [ ! -s "dav1d_call_gates_0.ld" ]; then echo "No link args as expected"; exit 0; fi; echo "Unexpected link args"; exit 1;'
*/

// Check that readelf shows exactly one executable segment

#include "dav1d.h"
#include <ia2.h>
#include <ia2_test_runner.h>

#include <signal.h>

INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

Dav1dContext c IA2_SHARED_DATA;
Dav1dSettings settings IA2_SHARED_DATA;
Dav1dPicture pic IA2_SHARED_DATA;
Dav1dContext c2 IA2_SHARED_DATA;

Test(type_confusion, normal) {
  fprintf(stderr, "&c = %p\n", &c);
  fprintf(stderr, "&settings = %p\n", &settings);
  fprintf(stderr, "&pic = %p\n", &pic);
  dav1d_open(&c, &settings);
  dav1d_get_picture(&c, &pic);
  dav1d_close(&c);
}

Test(type_confusion, type_confusion,
     .signal = SIGABRT, ) {
  dav1d_open(&c, &settings);
  // Try to use another `Dav1dContext` that hasn't been constructed/opened yet.
  dav1d_get_picture(&c2, &pic);
  dav1d_close(&c);
}

Test(type_confusion, type_confusion_2,
     .signal = SIGABRT, ) {
  dav1d_open(&c, &settings);
  // Try to use another `Dav1dContext` that hasn't been constructed/opened yet.
  dav1d_get_picture((Dav1dContext *)&pic, &pic);
  dav1d_close(&c);
}
