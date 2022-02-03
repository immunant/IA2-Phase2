#if __has_include("simple1_ia2.h")
#include "simple1_ia2.h"
#define MAIN_USE_IA2 1
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <ia2.h>

#include "hooks.h"
#include "simple1.h"

INIT_COMPARTMENT(0);

static HookFn exit_hook_fn = NULL;

HookFn get_exit_hook(void) {
  return exit_hook_fn;
}

void set_exit_hook(HookFn new_exit_hook_fn) {
  exit_hook_fn = new_exit_hook_fn;
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
#if MAIN_USE_IA2
    .read_cb = IA2_FNPTR_WRAPPER(main_read, _ZTSPFiiE),
    .write_cb = IA2_FNPTR_WRAPPER_VOID(main_write, _ZTSPFviE),
#else
    .read_cb = main_read,
    .write_cb = main_write,
#endif
  };

  struct Simple *s = simple_new(scb);
  if (s == NULL) {
    printf("Error allocating Simple\n");
    return -1;
  }

  srand(time(NULL));
#if MAIN_USE_IA2
  simple_foreach_v1(s, IA2_FNPTR_WRAPPER(main_map, _ZTSPFiiE));
  simple_reset(s);
  simple_foreach_v2(s, IA2_FNPTR_WRAPPER(main_map, _ZTSPFiiE));
#else
  simple_foreach_v1(s, main_map);
  simple_reset(s);
  simple_foreach_v2(s, main_map);
#endif
  simple_destroy(s);

  if (exit_hook_fn != NULL) {
    (*exit_hook_fn)();
  }

  return 0;
}
