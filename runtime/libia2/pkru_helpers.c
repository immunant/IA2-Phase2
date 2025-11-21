#if defined(IA2_LIBC_COMPARTMENT) && IA2_LIBC_COMPARTMENT

#include "ia2_internal.h"

#if defined(__x86_64__)
uint32_t ia2_read_pkru(void) {
  uint32_t pkru;
  __asm__ volatile("rdpkru" : "=a"(pkru) : "c"(0), "d"(0));
  return pkru;
}

void ia2_write_pkru(uint32_t pkru) {
  __asm__ volatile("wrpkru" : : "a"(pkru), "c"(0), "d"(0) : "memory");
}
#else
uint32_t ia2_read_pkru(void) {
  return 0;
}

void ia2_write_pkru(uint32_t pkru) {
  (void)pkru;
}
#endif

#endif // IA2_LIBC_COMPARTMENT
