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

void ia2_main(void) {
    ia2_register_compartment("main", 1, NULL);
    ia2_register_compartment("libtype_confusion_lib.so", 2, NULL);
}

Test(type_confusion, normal) {
  // `Dav1dContext` is opaque.
  Dav1dContext *c = dav1d_alloc(DAV1D_CONTEXT_SIZE);
  Dav1dSettings *settings = dav1d_alloc(sizeof(Dav1dSettings));
  Dav1dPicture *pic = dav1d_alloc(sizeof(Dav1dPicture));

  dav1d_open(c, settings);
  dav1d_get_picture(c, pic);
  dav1d_close(c);

  dav1d_free(pic);
  dav1d_free(settings);
  dav1d_free(c);
}

Test(type_confusion, uninitialized, .signal = SIGABRT) {
  Dav1dContext *c = dav1d_alloc(DAV1D_CONTEXT_SIZE);
  Dav1dContext *c2 = dav1d_alloc(DAV1D_CONTEXT_SIZE);
  Dav1dSettings *settings = dav1d_alloc(sizeof(Dav1dSettings));
  Dav1dPicture *pic = dav1d_alloc(sizeof(Dav1dPicture));

  dav1d_open(c, settings);
  // Try to use another `Dav1dContext` that hasn't been constructed/opened yet.
  dav1d_get_picture(c2, pic); // Will `SIGABRT`.
  dav1d_close(c);

  dav1d_free(pic);
  dav1d_free(settings);
  dav1d_free(c2);
  dav1d_free(c);
}

Test(type_confusion, wrong_type, .signal = SIGABRT) {
  Dav1dContext *c = dav1d_alloc(DAV1D_CONTEXT_SIZE);
  Dav1dSettings *settings = dav1d_alloc(sizeof(Dav1dSettings));
  Dav1dPicture *pic = dav1d_alloc(sizeof(Dav1dPicture));

  dav1d_open(c, settings);
  // Try to use another type (`Dav1dPicture`) instead.
  dav1d_get_picture((Dav1dContext *)pic, pic); // Will `SIGABRT`.
  dav1d_close(c);

  dav1d_free(pic);
  dav1d_free(settings);
  dav1d_free(c);
}

Test(type_confusion, null, .signal = SIGABRT) {
  Dav1dContext *c = dav1d_alloc(DAV1D_CONTEXT_SIZE);
  Dav1dSettings *settings = dav1d_alloc(sizeof(Dav1dSettings));
  Dav1dPicture *pic = dav1d_alloc(sizeof(Dav1dPicture));

  dav1d_open(c, settings);
  // Try to `NULL`.
  dav1d_get_picture(NULL, pic); // Will `SIGABRT`.
  dav1d_close(c);

  dav1d_free(pic);
  dav1d_free(settings);
  dav1d_free(c);
}

Test(type_confusion, use_after_free, .signal = SIGABRT) {
  Dav1dContext *c = dav1d_alloc(DAV1D_CONTEXT_SIZE);
  Dav1dSettings *settings = dav1d_alloc(sizeof(Dav1dSettings));
  Dav1dPicture *pic = dav1d_alloc(sizeof(Dav1dPicture));

  dav1d_open(c, settings);
  dav1d_close(c);
  // Try to use an already destructed `Dav1dContext`.
  dav1d_get_picture(c, pic); // Will `SIGABRT`.

  dav1d_free(pic);
  dav1d_free(settings);
  dav1d_free(c);
}
