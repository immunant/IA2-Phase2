#include "main_fn_ptr_ia2.h"
#include "simple1_fn_ptr_ia2.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <ia2.h>

#include "hook_ty.h"
#include "simple1.h"

INIT_COMPARTMENT;

// Redeclare the HookFn typedef since it becomes an opaque pointer in the
// rewritten headers.
typedef void (*HookFnInternal)(void);

static HookFnInternal exit_hook_fn = NULL;

HookFn get_exit_hook(void) {
  // We must wrap exit_hook_fn before passing it to another compartment.
  // This is enforced by the types in the rewritten headers.
  return IA2_FNPTR_WRAPPER_VOID(exit_hook_fn, _ZTSPFvvE, NO_PKEY);
}

void set_exit_hook(HookFn new_exit_hook_fn) {
  // We must unwrap new_exit_hook_fn since it comes from another compartment.
  // This is enforced because exit_hook_fn is a function pointer while HookFn is
  // opaque.
  exit_hook_fn = IA2_FNPTR_UNWRAPPER_VOID(new_exit_hook_fn, _ZTSPFvvE, NO_PKEY);
}

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

static void main_write(int x) {
  putchar(x);
}

static int main_map(int x) {
  return x ? (x ^ last_xor) : x;
}

int main() {
  struct SimpleCallbacks scb = {
    .read_cb = IA2_FNPTR_WRAPPER(main_read, _ZTSPFiiE, 0),
    .write_cb = IA2_FNPTR_WRAPPER_VOID(main_write, _ZTSPFviE, 0),
  };

  struct Simple *s = simple_new(scb);
  if (s == NULL) {
    printf("Error allocating Simple\n");
    return -1;
  }

  srand(time(NULL));
  simple_foreach_v1(s, IA2_FNPTR_WRAPPER(main_map, _ZTSPFiiE, 0));
  simple_reset(s);
  simple_foreach_v2(s, IA2_FNPTR_WRAPPER(main_map, _ZTSPFiiE, 0));
  simple_destroy(s);

  if (exit_hook_fn != NULL) {
    printf("%p\n%p\n", main, exit_hook_fn);
    (*exit_hook_fn)();
  }

  return 0;
}
