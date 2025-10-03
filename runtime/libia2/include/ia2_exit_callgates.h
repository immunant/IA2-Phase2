#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ia2_callgate_cookie {
  uint32_t saved_pkru;
  void *saved_sp;
} ia2_callgate_cookie;

ia2_callgate_cookie ia2_callgate_enter(int compartment, const char *symbol);
void ia2_callgate_exit(ia2_callgate_cookie cookie);

#ifdef __cplusplus
}
#endif
