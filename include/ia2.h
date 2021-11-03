#pragma once

#ifdef IA2_WRAPPER
#define IA2_WRAP_FUNCTION(name)
#else
#define IA2_WRAP_FUNCTION(name)                 \
    __asm__(".symver " #name ",__ia2_" #name "@IA2")
#endif

#define IA2_FNPTR_WRAPPER(target, ty) ({               \
  IA2_FNPTR_WRAPPER_##ty(IA2_fnptr_wrapper_##target) { \
    IA2_FNPTR_RETURN_##ty(__res) =                     \
      target(IA2_FNPTR_ARG_NAMES_##ty);                \
    return __res;                                      \
  }                                                    \
  (struct IA2_fnptr_##ty){                             \
    (char*)IA2_fnptr_wrapper_##target                  \
  };                                                   \
})

#define IA2_FNPTR_WRAPPER_VOID(target, ty) ({          \
  IA2_FNPTR_WRAPPER_##ty(IA2_fnptr_wrapper_##target) { \
    target(IA2_FNPTR_ARG_NAMES_##ty);                  \
  }                                                    \
  (struct IA2_fnptr_##ty){                             \
    (char*)IA2_fnptr_wrapper_##target                  \
  };                                                   \
})

#include "call_gates.h"

/**
 * Allocates a protection key and calls `pkey_mprotect` on all pages in the trusted compartment and
 */
__attribute__((constructor)) void initialize_heap_pkey(const uint8_t *heap_start, uintptr_t heap_len);
