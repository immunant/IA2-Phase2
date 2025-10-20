#pragma once

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
#else
#include <stddef.h>
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

uint32_t ia2_get_pkru();
size_t ia2_get_pkey();

#ifdef IA2_USE_PKRU_GATES
// Invalidate the PKRU cache - must be called after any PKRU write
// This is called from loader gate enter/exit and compartment transitions
void ia2_invalidate_pkru_cache();
#endif

#ifdef __cplusplus
}
#endif
