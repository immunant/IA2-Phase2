#pragma once
#include <stdio.h>
#include "pkey_init.h"

#define PKRU_UNTRUSTED 0xFFFFFFFC
#define NO_PKEY -1

#define IA2_SHARED_DATA __attribute__((section("ia2_shared_data")))


#ifdef IA2_WRAPPER
#define IA2_WRAP_FUNCTION(name)
#else
#define IA2_WRAP_FUNCTION(name)                 \
    __asm__(".symver " #name ",__ia2_" #name "@IA2")
#endif


// Reads the pkru state into the uint32_t `pkru`.
#define READ_PKRU(pkru)                                                        \
        __asm__ ("rdpkru"                                          \
            : "=a" (pkru)                                                      \
            : "c" (0)                                                          \
            : "edx");                                                          \


// Set the pkru state to the uint32_t `pkru`.
#define WRITE_PKRU(pkru)                                                       \
        __asm__("wrpkru"                                                       \
            :                                                                  \
            : "a" (pkru), "c" (0), "d" (0));


#ifdef LIBIA2_INSECURE

#define __libia2_gate_push(idx)
#define __libia2_gate_pop()

#else

#define __libia2_gate_push(idx)                                                \
    uint32_t new_pkru = PKRU_UNTRUSTED;                                        \
    if (idx != NO_PKEY) {                                                      \
        int32_t pkey = ((int32_t *)&IA2_INIT_DATA)[idx]; \
        if (pkey == PKEY_UNINITIALIZED) {                                      \
            printf("Entering another compartment without a protection key\n"); \
            exit(-1);                                                          \
        }                                                                      \
        new_pkru &= ~(3 << (2 * pkey));                                        \
    }                                                                          \
    uint32_t old_pkru;                                                         \
    READ_PKRU(old_pkru);                                                       \
    WRITE_PKRU(new_pkru);


#define __libia2_gate_pop()                                                \
    uint32_t pkru;                                                         \
    READ_PKRU(pkru);                                                       \
    if (pkru != new_pkru) {                                                \
        printf("PKRU changed inside compartment\n");                       \
        exit(-1);                                                          \
    }                                                                      \
    WRITE_PKRU(old_pkru);

#endif


// FIXME: All the IA2_FNPTR_* macros only work if `target` is a valid identifier unique to the binary.
#define IA2_FNPTR_WRAPPER(target, ty) ({               \
  IA2_FNPTR_WRAPPER_##ty(IA2_fnptr_wrapper_##target) { \
    __libia2_gate_push(NO_PKEY);                       \
    IA2_FNPTR_RETURN_##ty(__res) =                     \
      target(IA2_FNPTR_ARG_NAMES_##ty);                \
    __libia2_gate_pop();                               \
    return __res;                                      \
  }                                                    \
  (struct IA2_fnptr_##ty){                             \
    (char*)IA2_fnptr_wrapper_##target                  \
  };                                                   \
})

#define IA2_FNPTR_WRAPPER_VOID(target, ty) ({          \
  IA2_FNPTR_WRAPPER_##ty(IA2_fnptr_wrapper_##target) { \
    __libia2_gate_push(NO_PKEY);                       \
    target(IA2_FNPTR_ARG_NAMES_##ty);                  \
    __libia2_gate_pop();                               \
  }                                                    \
  (struct IA2_fnptr_##ty){                             \
    (char*)IA2_fnptr_wrapper_##target                  \
  };                                                   \
})

#define IA2_FNPTR_UNWRAPPER(target, ty) ({                           \
  IA2_FNPTR_WRAPPER_##ty(IA2_fnptr_wrapper_##target) {               \
    __libia2_gate_push(NO_PKEY);                                     \
    IA2_FNPTR_RETURN_##ty(__res) =                                   \
      ((IA2_FNPTR_TYPE_##ty)(target.ptr))(IA2_FNPTR_ARG_NAMES_##ty); \
    __libia2_gate_pop();                                             \
    return __res;                                                    \
  }                                                                  \
  IA2_fnptr_wrapper_##target;                                        \
})

#define IA2_FNPTR_UNWRAPPER_VOID(target, ty) ({                      \
  IA2_FNPTR_WRAPPER_##ty(IA2_fnptr_wrapper_##target) {               \
    __libia2_gate_push(NO_PKEY);                                     \
    ((IA2_FNPTR_TYPE_##ty)(target.ptr))(IA2_FNPTR_ARG_NAMES_##ty);   \
    __libia2_gate_pop();                                             \
  }                                                                  \
  IA2_fnptr_wrapper_##target;                                        \
})

// We must declare the sections used to pad the end of each program header
// segment to make sure their rwx permissions match the segment they're placed
// in. Otherwise if the padding sections are declared in the linker script
// without any input sections they and their corresponding segment will default
// to rwx. We avoid using .balign to align the sections at the start of each
// segment because it inserts a fill value (defaults to 0) which may break some
// sections (e.g.  insert null pointers into .init_array).
#define NEW_SECTION(name) \
    __asm__(".section " #name "\n\
             .previous");

// Since `initialize_heap_pkey` is defined in libia2.so adding a constructor
// attribute to its declaration won't put it in the main program's .ctors
// section, so we have to create this wrapper instead.
#define INIT_COMPARTMENT INIT_COMPARTMENT_N(0)

#define INIT_COMPARTMENT_N(x)                              \
    NEW_SECTION(".fini_padding");                          \
    NEW_SECTION(".rela.plt_padding");                      \
    NEW_SECTION(".eh_frame_padding");                      \
    NEW_SECTION(".bss_padding");                           \
    __attribute__((constructor)) static void init_pkey_ctor() {   \
        initialize_compartment(x, &init_pkey_ctor);        \
    }

