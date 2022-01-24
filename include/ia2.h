#pragma once

#define IA2_SHARED_DATA __attribute__((section("ia2_shared_data")))
#ifdef IA2_WRAPPER
#define IA2_WRAP_FUNCTION(name)
#else
#define IA2_WRAP_FUNCTION(name)                 \
    __asm__(".symver " #name ",__ia2_" #name "@IA2")
#endif

// FIXME: All the IA2_FNPTR_* macros only work if `target` is a valid identifier unique to the binary.
#define IA2_FNPTR_WRAPPER(target, ty) ({               \
  IA2_FNPTR_WRAPPER_##ty(IA2_fnptr_wrapper_##target) { \
    __libia2_untrusted_gate_pop_ptr();                 \
    IA2_FNPTR_RETURN_##ty(__res) =                     \
      target(IA2_FNPTR_ARG_NAMES_##ty);                \
    __libia2_untrusted_gate_push_ptr();                \
    return __res;                                      \
  }                                                    \
  (struct IA2_fnptr_##ty){                             \
    (char*)IA2_fnptr_wrapper_##target                  \
  };                                                   \
})

#define IA2_FNPTR_WRAPPER_VOID(target, ty) ({          \
  IA2_FNPTR_WRAPPER_##ty(IA2_fnptr_wrapper_##target) { \
    __libia2_untrusted_gate_pop_ptr();                 \
    target(IA2_FNPTR_ARG_NAMES_##ty);                  \
    __libia2_untrusted_gate_push_ptr();                \
  }                                                    \
  (struct IA2_fnptr_##ty){                             \
    (char*)IA2_fnptr_wrapper_##target                  \
  };                                                   \
})

#define IA2_FNPTR_UNWRAPPER(target, ty) ({                \
  IA2_FNPTR_WRAPPER_##ty(IA2_fnptr_wrapper_##target) {    \
    __libia2_untrusted_gate_push_ptr();                \
    IA2_FNPTR_RETURN_##ty(__res) =                     \
      ((IA2_FNPTR_TYPE_##ty)(target.ptr))(IA2_FNPTR_ARG_NAMES_##ty); \
    __libia2_untrusted_gate_pop_ptr();                 \
    return __res;                                      \
  }                                                    \
  IA2_fnptr_wrapper_##target;        \
})

#define IA2_FNPTR_UNWRAPPER_VOID(target, ty) ({           \
  IA2_FNPTR_WRAPPER_##ty(IA2_fnptr_wrapper_##target) {    \
    __libia2_untrusted_gate_push_ptr();                \
    ((IA2_FNPTR_TYPE_##ty)(target.ptr))(IA2_FNPTR_ARG_NAMES_##ty);   \
    __libia2_untrusted_gate_pop_ptr();                 \
  }                                                    \
  IA2_fnptr_wrapper_##target;             \
})

#include "call_gates.h"

// The init heap ctor should only be defined in the main program
#ifndef IA2_WRAPPER

// For untrusted -> trusted indirect calls we can't call
// `__libia2_untrusted_gate_pop` through the main program's PLT stub since the
// pkru state is untrusted. Instead we call the gates through these function
// pointers which are placed in sections of the main program that are ignored by
// libia2's pkey_mprotect calls.
const void (*__libia2_untrusted_gate_push_ptr)(void) IA2_SHARED_DATA = &__libia2_untrusted_gate_push;
const void (*__libia2_untrusted_gate_pop_ptr)(void) IA2_SHARED_DATA = &__libia2_untrusted_gate_pop;

// We must declare the sections used to pad the end of each program header
// segment to make sure their rwx permissions match the segment they're placed
// in. Otherwise the padding sections will be declared in the linker script
// which defaults to rwx for the section and the corresponding segment. We avoid
// using .balign to align the sections at the start of each segment because it
// inserts a fill value (defaults to 0) which may break some sections (e.g.
// insert null pointers into .init_array).
#define NEW_SECTION(name) \
    __asm__(".section " #name "\n\
             .previous");

NEW_SECTION(".fini_padding");
NEW_SECTION(".rela.plt_padding");
NEW_SECTION(".eh_frame_padding");
NEW_SECTION(".bss_padding");
NEW_SECTION("ia2_shared_data");

// Since `initialize_heap_pkey` is defined in libia2.so adding a constructor
// attribute to its declaration won't put it in the main program's .ctors
// section, so we have to create this wrapper instead.
__attribute__((constructor)) void init_heap_ctor() {
    initialize_heap_pkey(NULL, 0);
}
#endif

