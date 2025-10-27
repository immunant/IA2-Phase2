#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Lightweight snapshot of the caller's privilege state used to unwind an exit
 * call gate. The saved PKRU and stack pointer allow us to restore the original
 * compartment context after executing code in the libc compartment.
 */
typedef struct ia2_callgate_cookie {
  uint32_t saved_pkru;
  void *saved_sp;
} ia2_callgate_cookie;

/**
 * Switches into the exit (libc) compartment and returns a cookie capturing the
 * caller's PKRU and stack pointer so that the original compartment can resume
 * once the exit call gate completes.
 */
ia2_callgate_cookie ia2_callgate_enter(void);

#ifdef __cplusplus
}
#endif
