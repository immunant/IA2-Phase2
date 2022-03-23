#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <ia2.h>

#include "hooks.h"
#include "simple1.h"

INIT_RUNTIME(1);
INIT_COMPARTMENT(0);

// libsimple1 checks if the function pointer is NULL
static HookFn exit_hook_fn = IA2_NULL_FNPTR(_ZTSPFvvE);

HookFn get_exit_hook(void) { return exit_hook_fn; }

void set_exit_hook(HookFn new_exit_hook_fn) { exit_hook_fn = new_exit_hook_fn; }

// Secret values: a secret string and decryption value.
// The untrusted compartment should not be able to read these.
static const char secret_string[] = "This is a secret.\n";
static int last_xor;

static int main_read(int i) {
  if (i >= sizeof(secret_string)) {
    return 0;
  }

  int x = secret_string[i];
  last_xor = rand();
  return x ? (x ^ last_xor) : x;
}

static void main_write(int x) { putchar(x); }

static int main_map(int x) { return x ? (x ^ last_xor) : x; }

int main() {
  // These will be called from untrusted code but may access trusted compartment
  // 0
  struct SimpleCallbacks scb = {
      .read_cb = IA2_FNPTR_WRAPPER(main_read, _ZTSPFiiE, UNTRUSTED, 0),
      .write_cb = IA2_FNPTR_WRAPPER(main_write, _ZTSPFviE, UNTRUSTED, 0),
  };

  struct Simple *s = simple_new(scb);
  if (s == NULL) {
    printf("Error allocating Simple\n");
    return -1;
  }

  srand(time(NULL));
  // These will be called from untrusted code but may access trusted compartment
  // 0
  simple_foreach_v1(s, IA2_FNPTR_WRAPPER(main_map, _ZTSPFiiE, UNTRUSTED, 0));
  simple_reset(s);
  simple_foreach_v2(s, IA2_FNPTR_WRAPPER(main_map, _ZTSPFiiE, UNTRUSTED, 0));
  simple_destroy(s);

  // We need to check if exit_hook_fn is NULL since IA2_FNPTR_UNWRAPPER always
  // returns a non-null pointer. Since it's an opaque pointer, we use this macro
  // instead of directly comparing with NULL.
  if (!IA2_FNPTR_IS_NULL(exit_hook_fn)) {
    // These will be called from the trusted compartment (TC) but came from the
    // untrusted compartment (UC) so it should not have access to compartment 0.
    // If the wrapped function was defined by TC then passed to UC then passed
    // back to TC, the function must've been wrapped before TC passed it to UC.
    // That wrapper would make sure the function ran with TC's pkey, not the one
    // defined below. In this test however, exit_hook_fn is initially NULL and
    // libsimple1 defines the exit hook so it will always run as UNTRUSTED.
    IA2_FNPTR_UNWRAPPER(exit_hook_fn, _ZTSPFvvE, 0, UNTRUSTED)();
  }

  return 0;
}
