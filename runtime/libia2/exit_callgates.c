#include "ia2_exit_callgates.h"

#include "ia2.h"
#include "ia2_internal.h"

#include <assert.h>

// -----------------------------------------------------------------------------
// Exit call gates
// -----------------------------------------------------------------------------
//
// `libia2` interposes libc’s `__cxa_finalize` by compiling its own
// `__wrap___cxa_finalize` into the runtime.  Because the runtime is built
// without the rewriter, there are no generated call-gate stubs available inside
// `libia2`.  These helpers let the runtime perform the same PKRU/stack swap the
// generated stubs would normally do:
//   1. save the caller’s PKRU (and, on x86_64, the caller’s stack pointer),
//   2. switch to the exit-compartment stack,
//   3. install the exit-compartment PKRU mask, and
//   4. return the saved state as an `ia2_callgate_cookie` so
//      `ia2_callgate_exit` can restore everything when control comes back.
//
// The exit compartment is currently hard-coded to pkey 1 (where libc/ld.so
// live).  If the runtime ever supports multiple exit pkeys we should thread the
// target pkey through these helpers instead of using IA2_EXIT_COMPARTMENT_PKEY.

// Pushes the current thread into the exit compartment and returns the saved
// execution context. Callers must treat the returned cookie as opaque and feed
// it back to `ia2_callgate_exit` once the exit-compartment work is finished.
//
// On x86_64 we also swap to the exit stack, because callers may be executing on
// a compartment-local stack that libc/ld.so is not permitted to touch.
ia2_callgate_cookie ia2_callgate_enter(void) {
  ia2_callgate_cookie cookie;
  cookie.saved_pkru = ia2_read_pkru();
  cookie.saved_sp = NULL;

#if defined(__x86_64__)
  void *saved_sp = NULL;
  __asm__ volatile("mov %%rsp, %0" : "=r"(saved_sp));
  cookie.saved_sp = saved_sp;

  void **exit_stack_slot = ia2_stackptr_for_compartment(IA2_EXIT_COMPARTMENT_PKEY);
  void *exit_sp = exit_stack_slot ? *exit_stack_slot : NULL;
  assert(exit_sp && "exit compartment stack must be initialized");
  if (exit_sp) {
    __asm__ volatile("mov %0, %%rsp" : : "r"(exit_sp) : "memory");
  }
#endif

  uint32_t exit_pkru = PKRU(IA2_EXIT_COMPARTMENT_PKEY);
  ia2_write_pkru(exit_pkru);
#if IA2_DEBUG && defined(__x86_64__)
  {
    uint32_t observed_pkru = ia2_read_pkru();
    assert(observed_pkru == exit_pkru && "exit call gate failed to switch PKRU to exit compartment");
  }
#endif

  return cookie;
}

// Restores the stack pointer (if it was switched) and the caller's PKRU. Every
// `ia2_callgate_enter` must be paired with exactly one call to this function; a
// missing or double call will leave the thread with an invalid stack or the
// wrong PKRU.
void ia2_callgate_exit(ia2_callgate_cookie cookie) {
#if defined(__x86_64__)
  if (cookie.saved_sp) {
    __asm__ volatile("mov %0, %%rsp" : : "r"(cookie.saved_sp) : "memory");
  }
#endif
  ia2_write_pkru(cookie.saved_pkru);
#if IA2_DEBUG && defined(__x86_64__)
  {
    uint32_t observed_pkru = ia2_read_pkru();
    assert(observed_pkru == cookie.saved_pkru && "exit call gate failed to restore caller PKRU");
  }
#endif
}
