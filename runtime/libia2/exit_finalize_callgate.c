#include "ia2_exit_callgates.h"

#include <stddef.h>

extern void __real___cxa_finalize(void *dso_handle);

// Exit compartment is always pkey 1 (libc compartment)
#define EXIT_COMPARTMENT_PKEY 1

void __wrap___cxa_finalize(void *dso_handle) {
  ia2_callgate_cookie cookie = ia2_callgate_enter();
  __real___cxa_finalize(dso_handle);
  ia2_callgate_exit(cookie);
}
