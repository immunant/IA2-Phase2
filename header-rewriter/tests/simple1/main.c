#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <ia2.h>

#include "hooks.h"
#include "simple1.h"

INIT_RUNTIME(1);
INIT_COMPARTMENT(1);

// libsimple1 checks if the function pointer is NULL. To initialize this to a
// function defined in this binary, we'd need to define a wrapper with
// IA2_DEFINE_WRAPPER with target pkey 1, then use IA2_WRAPPER.
static HookFn exit_hook_fn = NULL;

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
      .read_cb = main_read,
      .write_cb = main_write,
  };

  struct Simple *s = simple_new(scb);
  if (s == NULL) {
    printf("Error allocating Simple\n");
    return -1;
  }

  srand(time(NULL));
  // These will be called from untrusted code but may access trusted compartment
  // 0
  simple_foreach_v1(s, main_map);
  simple_reset(s);
  simple_foreach_v2(s, main_map);
  simple_destroy(s);

  // We need to check if exit_hook_fn is NULL since IA2_CALL always
  // returns a non-null pointer. Since it's an opaque pointer, we use this macro
  // instead of directly comparing with NULL.
  if (!exit_hook_fn) {
    // Creates a wrapper that assumes the caller has pkey 0 and the callee is
    // untrusted since libsimple1 sets the value of exit_hook_fn. If
    // exit_hook_fn were to point to a function defined in this binary, it must
    // be a wrapped function with an untrusted caller and callee with pkey 0.
    exit_hook_fn();
  }

  return 0;
}
