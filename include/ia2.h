#pragma once
#include <link.h>
#include <sys/mman.h>
#include <stdio.h>
#include "pkey_init.h"

// Attribute for variables that can be accessed from any untrusted compartments.
#define IA2_SHARED_DATA __attribute__((section("ia2_shared_data")))

#ifdef IA2_WRAPPER
#define IA2_WRAP_FUNCTION(name)
#else
#define IA2_WRAP_FUNCTION(name) __asm__(".symver " #name ",__ia2_" #name "@IA2")
#endif

#define XSTR(s) STR(s)
#define STR(s) #s
// TODO: Incorporate __FILE_NAME__ and __COUNTER__ if possible to make this more
// flexible.
#define UNIQUE_STR(s) s "_line_" XSTR(__LINE__)

// On linux protection key 0 is the default for anything not covered by
// pkey_mprotect so untrusted compartments have the lower 2 bits of PKRU cleared
#define PKRU_UNTRUSTED 0xFFFFFFFC

// Set PKRU to untrusted using rax, r10 and r11 as scratch registers.
#define WRPKRU(pkey)                        \
    "movq %rcx, %r10\n"                     \
    "movq %rdx, %r11\n"                     \
    "movl $" XSTR(pkey) ", %eax\n"           \
    "xorl %ecx, %ecx\n"                     \
    "xorl %edx, %edx\n"                     \
    "wrpkru\n"                              \
    "movq %r10, %rcx\n"                     \
    "movq %r11, %rdx\n"

#ifdef LIBIA2_INSECURE
#define GATE(x)
#define INIT_COMPARTMENT(n)
#define INIT_RUNTIME(n)
#else
#define GATE(pkey) _GATE(pkey)
#define INIT_COMPARTMENT(n) _INIT_COMPARTMENT(n)
#define INIT_RUNTIME(n) _INIT_RUNTIME(n)
#endif

#define _GATE(pkey) GATE_##pkey

#define GATE_UNTRUSTED   WRPKRU(0xFFFFFFFC)
#define GATE_0           WRPKRU(0xFFFFFFF0)
#define GATE_1           WRPKRU(0xFFFFFFCC)
#define GATE_2           WRPKRU(0xFFFFFF3C)
#define GATE_3           WRPKRU(0xFFFFFCFC)
#define GATE_4           WRPKRU(0xFFFFF3FC)
#define GATE_5           WRPKRU(0xFFFFCFFC)
#define GATE_6           WRPKRU(0xFFFF3FFC)
#define GATE_7           WRPKRU(0xFFFCFFFC)
#define GATE_8           WRPKRU(0xFFF3FFFC)
#define GATE_9           WRPKRU(0xFFCFFFFC)
#define GATE_10          WRPKRU(0xFF3FFFFC)
#define GATE_11          WRPKRU(0xFCFFFFFC)
#define GATE_12          WRPKRU(0xF3FFFFFC)
#define GATE_13          WRPKRU(0xCFFFFFFC)
#define GATE_14          WRPKRU(0x3FFFFFFC)

// Takes a function pointer `target` and returns an opaque pointer for its call gate wrapper.
#define IA2_FNPTR_WRAPPER(target, ty, caller_pkey, target_pkey)    ({    \
    static char *target_ptr __asm__(UNIQUE_STR(#target)) IA2_SHARED_DATA;                \
    static void *wrapper __asm__("__ia2_" UNIQUE_STR(#target));          \
    target_ptr = (char *)target;                                         \
    __asm__(IA2_FNPTR_WRAPPER_##ty(target, caller_pkey, target_pkey));  \
    (struct IA2_fnptr_##ty) {                                            \
        (char *)&wrapper                                                 \
    };                                                                   \
})

// Takes an opaque pointer `target` and returns a function pointer for its call gate wrapper.
#define IA2_FNPTR_UNWRAPPER(target, ty, caller_pkey, target_pkey)  ({    \
    static char *target_ptr __asm__(UNIQUE_STR(#target)) IA2_SHARED_DATA;                \
    static void *wrapper __asm__("__ia2_" UNIQUE_STR(#target));          \
    target_ptr = target.ptr;                                             \
    __asm__(IA2_FNPTR_WRAPPER_##ty(target, caller_pkey, target_pkey));   \
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
