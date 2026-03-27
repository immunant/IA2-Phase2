#include <shared_tls.h>

#ifndef TLS_REPRO_SHARED_TLS_BYTES
#define TLS_REPRO_SHARED_TLS_BYTES 0x1020
#endif
#if TLS_REPRO_SHARED_TLS_BYTES < 4
#error "TLS_REPRO_SHARED_TLS_BYTES must be >= 4"
#endif

#define TLS_REPRO_SHARED_TLS_U32_COUNT \
  ((TLS_REPRO_SHARED_TLS_BYTES + sizeof(uint32_t) - 1) / sizeof(uint32_t))

__thread uint32_t shared_tls_data[TLS_REPRO_SHARED_TLS_U32_COUNT] = {0};

uint32_t shared_tls_bump(void) {
  shared_tls_data[0]++;
  return shared_tls_data[0];
}

uintptr_t shared_tls_addr(void) {
  return (uintptr_t)&shared_tls_data[0];
}
