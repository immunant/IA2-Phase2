#include "ia2_exit_callgates.h"
#include "ia2_internal.h"

#include <stddef.h>

extern void __real___cxa_finalize(void *dso_handle);

void __wrap___cxa_finalize(void *dso_handle) {
  ia2_callgate_cookie cookie = ia2_callgate_enter();
  __real___cxa_finalize(dso_handle);
  ia2_callgate_exit(cookie);
}
