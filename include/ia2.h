#pragma once
#include <link.h>
#include <sys/mman.h>
#include <stdio.h>
#include "pkey_init.h"
#include "scrub_registers.h"

// Attribute for variables that can be accessed from any untrusted compartments.
#define IA2_SHARED_DATA __attribute__((section("ia2_shared_data")))

#define IA2_WRAP_FUNCTION(name) __asm__(".symver " #name ",__ia2_" #name "@IA2")

#define XSTR(s) STR(s)
#define STR(s) #s
// TODO: Incorporate __FILE_NAME__ and __COUNTER__ if possible to make this more
// flexible.
#define UNIQUE_STR(s) s "_line_" XSTR(__LINE__)

#ifdef LIBIA2_INSECURE
#define INIT_COMPARTMENT(n)
#define INIT_RUNTIME(n)
#define IA2_WRPKRU
#else
#define INIT_COMPARTMENT(n) _INIT_COMPARTMENT(n)
#define INIT_RUNTIME(n) _INIT_RUNTIME(n)
#define IA2_WRPKRU "wrpkru"
#endif

#define PKRU(n) _PKRU(n)
#define _PKRU(n) PKRU_##n
// On linux protection key 0 is the default for anything not covered by
// pkey_mprotect so untrusted compartments have the lower 2 bits of PKRU cleared
#define PKRU_UNTRUSTED "0xFFFFFFFC"
// The PKRU value for protection key N has bits 2(N + 1) and 2(N + 1) + 1 clear
// to allow read and write access to compartment N. The lowest two bits should
// also be cleared since we should always have access to memory not covered by
// pkey_mprotect.
#define PKRU_0           "0xFFFFFFF0"
#define PKRU_1           "0xFFFFFFCC"
#define PKRU_2           "0xFFFFFF3C"
#define PKRU_3           "0xFFFFFCFC"
#define PKRU_4           "0xFFFFF3FC"
#define PKRU_5           "0xFFFFCFFC"
#define PKRU_6           "0xFFFF3FFC"
#define PKRU_7           "0xFFFCFFFC"
#define PKRU_8           "0xFFF3FFFC"
#define PKRU_9           "0xFFCFFFFC"
#define PKRU_10          "0xFF3FFFFC"
#define PKRU_11          "0xFCFFFFFC"
#define PKRU_12          "0xF3FFFFFC"
#define PKRU_13          "0xCFFFFFFC"
#define PKRU_14          "0x3FFFFFFC"

// Takes a function pointer `target` and returns an opaque pointer for its call gate wrapper.
#define IA2_FNPTR_WRAPPER(target, ty, caller_pkey, target_pkey)    ({    \
    static char *target_ptr __asm__(UNIQUE_STR(#target))                 \
                            __attribute__((used));                       \
    static void *wrapper __asm__("__ia2_" UNIQUE_STR(#target));          \
    target_ptr = (char *)target;                                         \
    __asm__(IA2_FNPTR_WRAPPER_##ty(target, caller_pkey, target_pkey));   \
    (struct IA2_fnptr_##ty) {                                            \
        (char *)&wrapper                                                 \
    };                                                                   \
})

// Takes an opaque pointer `target` and returns a function pointer for its call gate wrapper.
#define IA2_FNPTR_UNWRAPPER(target, ty, caller_pkey, target_pkey)  ({    \
    static char *target_ptr __asm__(UNIQUE_STR(#target))                 \
                            __attribute__((used));                       \
    static void *wrapper __asm__("__ia2_" UNIQUE_STR(#target));          \
    target_ptr = target.ptr;                                             \
    __asm__(IA2_FNPTR_UNWRAPPER_##ty(target, caller_pkey, target_pkey)); \
    (IA2_FNPTR_TYPE_##ty)&wrapper;                                       \
})

// We must declare the sections used to pad the end of each program header
// segment to make sure their rwx permissions match the segment they're placed
// in. Otherwise if the padding sections are declared in the linker script
// without any input sections they and their corresponding segment will default
// to rwx. We avoid using .balign to align the sections at the start of each
// segment because it inserts a fill value (defaults to 0) which may break some
// sections (e.g.  insert null pointers into .init_array).
#define NEW_SECTION(name)          \
    __asm__(".section " #name "\n" \
            ".previous");

// Initializes a compartment with protection key `n` when the ELF invoking this
// macro is loaded. This must only be called once for each key. The copmartment
// includes all segments in the ELF except the `ia2_shared_data` section, if one
// exists.
#define _INIT_COMPARTMENT(n)                                                              \
    NEW_SECTION(".fini_padding");                                                         \
    NEW_SECTION(".rela.plt_padding");                                                     \
    NEW_SECTION(".eh_frame_padding");                                                     \
    NEW_SECTION(".bss_padding");                                                          \
    extern int ia2_n_pkeys;                                                               \
    __attribute__((constructor)) static void init_pkey_ctor() {                           \
        if (ia2_n_pkeys != 0) {                                                           \
            for (int pkey = 1; pkey <= ia2_n_pkeys; pkey++) {                             \
                int allocated = pkey_alloc(0, 0);                                         \
                if (allocated != pkey) {                                                  \
                    printf("Failed to allocate protection keys in the expected order\n"); \
                    exit(-1);                                                             \
                }                                                                         \
            }                                                                             \
            ia2_n_pkeys = 0;                                                              \
        }                                                                                 \
        struct PhdrSearchArgs args = {                                                    \
            .pkey = n + 1,                                                                \
            .address = &init_pkey_ctor,                                                   \
        };                                                                                \
        dl_iterate_phdr(protect_pages, &args);                                            \
    }

// Defines the number of protection keys that need to be allocated
#define _INIT_RUNTIME(n) int ia2_n_pkeys = n;
