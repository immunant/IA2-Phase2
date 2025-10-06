#include "ia2_exit_callgates.h"

#include "ia2.h"
#include "ia2_internal.h"

#include <assert.h>

// Exit compartment is always pkey 1 (libc compartment)
#define EXIT_COMPARTMENT_PKEY 1

ia2_callgate_cookie ia2_callgate_enter(void) {
  ia2_callgate_cookie cookie;
  cookie.saved_pkru = ia2_read_pkru();
  cookie.saved_sp = NULL;

#if defined(__x86_64__)
  void *saved_sp = NULL;
  __asm__ volatile("mov %%rsp, %0" : "=r"(saved_sp));
  cookie.saved_sp = saved_sp;

  void **exit_stack_slot = ia2_stackptr_for_compartment(EXIT_COMPARTMENT_PKEY);
  void *exit_sp = exit_stack_slot ? *exit_stack_slot : NULL;
  assert(exit_sp && "exit compartment stack must be initialized");
  if (exit_sp) {
    __asm__ volatile("mov %0, %%rsp" : : "r"(exit_sp) : "memory");
  }
#endif

  uint32_t exit_pkru = PKRU(EXIT_COMPARTMENT_PKEY);
  ia2_write_pkru(exit_pkru);
#if IA2_DEBUG && defined(__x86_64__)
  {
    uint32_t observed_pkru = ia2_read_pkru();
    assert(observed_pkru == exit_pkru && "exit call gate failed to switch PKRU to exit compartment");
  }
#endif

  return cookie;
}

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
