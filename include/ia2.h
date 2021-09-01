#pragma once

#ifdef IA2_WRAPPER
#define IA2_WRAP_FUNCTION(name)
#else
#define IA2_WRAP_FUNCTION(name)                 \
    __asm__(".symver " #name ",__ia2_" #name "@IA2")
#endif

#define IA2_USED __attribute__((used))

#define IA2_ICALL_WRAPPER(target, ty) ({            \
  IA2_FNPTR_RETURN_##ty                             \
  IA2_icall_wrapper_##target(IA2_FNPTR_ARGS_##ty) { \
    IA2_FNPTR_RETURN_##ty __res =                   \
      target(IA2_FNPTR_ARG_NAMES_##ty);             \
    return __res;                                   \
  }                                                 \
  (struct IA2_fnptr_##ty){                          \
    (char*)IA2_icall_wrapper_##target               \
  };                                                \
})

#define IA2_ICALL_WRAPPER_VOID(target, ty) ({       \
  void                                              \
  IA2_icall_wrapper_##target(IA2_FNPTR_ARGS_##ty) { \
    target(IA2_FNPTR_ARG_NAMES_##ty);             \
  }                                                 \
  (struct IA2_fnptr_##ty){                          \
    (char*)IA2_icall_wrapper_##target               \
  };                                                \
})
