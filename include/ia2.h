#pragma once

#ifdef IA2_WRAPPER
#define IA2_WRAP_FUNCTION(name)
#else
#define IA2_WRAP_FUNCTION(name)                 \
    __asm__(".symver " #name ",__ia2_" #name "@IA2")
#endif

#define IA2_FNPTR_WRAPPER(target, ty) ({               \
  __attribute__((section("ia2_call_gates")))           \
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
  __attribute__((section("ia2_call_gates")))           \
  IA2_FNPTR_WRAPPER_##ty(IA2_fnptr_wrapper_##target) { \
    target(IA2_FNPTR_ARG_NAMES_##ty);                  \
  }                                                    \
  (struct IA2_fnptr_##ty){                             \
    (char*)IA2_fnptr_wrapper_##target                  \
  };                                                   \
})

#include "call_gates.h"

// The init heap ctor should only be defined in the main program
#ifndef IA2_WRAPPER
// Since `initialize_heap_pkey` is defined in libia2.so adding a constructor
// attribute to its declaration won't put it in the main program's .ctors
// section, so we have to create this wrapper instead.
__attribute__((constructor)) void init_heap_ctor() {
    initialize_heap_pkey(NULL, 0);
}
#endif

