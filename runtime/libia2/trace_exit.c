#include "ia2.h"

#include <stdint.h>
#include <stdio.h>

void ia2_trace_exit_record(int caller_pkey, int target_pkey, uint32_t pkru_value) {
#if IA2_TRACE_EXIT
  fprintf(stderr,
          "[TRACE_EXIT] caller_pkru=0x%08x caller_pkey=%d target_pkey=%d\n",
          pkru_value, caller_pkey, target_pkey);
#else
  (void)caller_pkey;
  (void)target_pkey;
  (void)pkru_value;
#endif
}
