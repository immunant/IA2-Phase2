#pragma once

#ifdef IA2_WRAPPER
#define IA2_WRAP_FUNCTION(name)
#else
#define IA2_WRAP_FUNCTION(name)                 \
    __asm__(".symver " #name ",__ia2_" #name "@IA2")
#endif

#define IA2_USED __attribute__((used))

#define IA2_ICALL_WRAPPER(target, ty)  \
  asm (                                \
    "IA2_icall_wrapper_" #target ":\n" \
    "call " #target "\n"               \
    "ret\n"                            \
  );                                   \
  extern char IA2_icall_wrapper_##target[]

#define IA2_ICALL_WRAPPED(target, ty) \
  ((struct IA2_fnptr_##ty){ IA2_icall_wrapper_##target })
