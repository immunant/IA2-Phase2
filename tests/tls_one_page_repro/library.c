#include <ia2.h>
#include <library.h>
#include <shared_tls.h>
#include <stdint.h>

#define IA2_COMPARTMENT 2
#include <ia2_compartment_init.inc>

#ifndef TLS_REPRO_LIB2_TLS_BYTES
#define TLS_REPRO_LIB2_TLS_BYTES 1
#endif
#if TLS_REPRO_LIB2_TLS_BYTES < 1
#error "TLS_REPRO_LIB2_TLS_BYTES must be >= 1"
#endif

__thread uint8_t lib2_tls_marker[TLS_REPRO_LIB2_TLS_BYTES] = {0};

uint32_t lib2_call_shared_tls_bump(void) {
  lib2_tls_marker[0]++;
  return shared_tls_bump();
}

uintptr_t lib2_get_shared_tls_addr(void) {
  return shared_tls_addr();
}

uintptr_t lib2_get_local_tls_addr(void) {
  return (uintptr_t)&lib2_tls_marker[0];
}
