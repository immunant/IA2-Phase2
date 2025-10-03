#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ia2_callgate_cookie {
  uint32_t saved_pkru;
  void *saved_sp;
#ifdef IA2_TRACE_EXIT
  int compartment;
  const char *symbol;
#endif
} ia2_callgate_cookie;

ia2_callgate_cookie ia2_callgate_enter(int compartment, const char *symbol);
void ia2_callgate_exit(ia2_callgate_cookie cookie);

uint64_t ia2_exit_callgate_enter_count(void);
uint64_t ia2_exit_callgate_exit_count(void);

#ifdef __cplusplus
}
#endif
