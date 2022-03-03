#pragma once
#include <link.h>
#include <sys/mman.h>
#include <stdio.h>

// This typedef is required for cbindgen
typedef struct dl_phdr_info dl_phdr_info;

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

// Suffix for GATE_ macro for untrusted gate code
#define NO_PKEY UNTRUSTED

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
#define DISABLE_PKRU
#else
#define GATE(pkey) _GATE(pkey)
#define DISABLE_PKRU     WRPKRU(0)

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

// FIXME: This doesn't support arguments on the stack or in r9 (the last
// register). Also doesn't support returning 128-bit values (rdx).
#define INDIRECT_WRAPPER(target, caller_pkey, target_pkey)                     \
    /* Jump to a subsection of .text to prevent inlining this function */      \
    ".text 1\n"                                                                \
    /* This is the symbol name that will appear in the executable */           \
    "__ia2_" UNIQUE_STR(#target) "_wrapper:\n"                                 \
    /* Set the value of the wrapper name used by asm to this location */       \
    ".equ __ia2_" UNIQUE_STR(#target) ",.\n"                                   \
    DISABLE_PKRU                                                               \
    "movq " UNIQUE_STR(#target) "@GOTPCREL(%rip), %r9\n"                \
    "movq (%r9), %r9\n"                                                 \
    /* Switch PKRU to the target compartment */                                \
    GATE(target_pkey)                                                          \
    "callq *%r9\n"                                                                \
    /* Save return value before toggling PKRU */                               \
    "movq %rax, %r9\n"                                                            \
    GATE(caller_pkey)                                                          \
    /* Put return value back in the right register */                          \
    "movq %r9, %rax\n"                                                            \
    "ret\n"                                                                    \
    /* Jump back to the previous location in .text */                          \
    ".previous\n"                                                              \

// Takes a function pointer `target` and returns an opaque pointer for its call gate wrapper.
#define IA2_FNPTR_WRAPPER(target, ty, caller_pkey, target_pkey)    ({    \
    static char *target_ptr __asm__(UNIQUE_STR(#target));                \
    static void *wrapper __asm__("__ia2_" UNIQUE_STR(#target));          \
    target_ptr = (char *)target;                                         \
    __asm__(INDIRECT_WRAPPER(target, caller_pkey, target_pkey));         \
    (struct IA2_fnptr_##ty) {                                            \
        (char *)&wrapper                                                 \
    };                                                                   \
})

// Takes an opaque pointer `target` and returns a function pointer for its call gate wrapper.
#define IA2_FNPTR_UNWRAPPER(target, ty, caller_pkey, target_pkey)  ({    \
    static char *target_ptr __asm__(UNIQUE_STR(#target));                \
    static void *wrapper __asm__("__ia2_" UNIQUE_STR(#target));          \
    target_ptr = target.ptr;                                             \
    __asm__(INDIRECT_WRAPPER(target, caller_pkey, target_pkey));         \
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

// Initializes a compartment with the nth key in IA2_INIT_DATA when the ELF
// invoking this is loaded. This must only be called once for each key. The
// compartment includes all segments in the ELF except the `ia2_shared_data`
// section, if one exists.
#define INIT_COMPARTMENT(n)                                          \
    NEW_SECTION(".fini_padding");                                    \
    NEW_SECTION(".rela.plt_padding");                                \
    NEW_SECTION(".eh_frame_padding");                                \
    NEW_SECTION(".bss_padding");                                     \
    struct IA2PhdrSearchArgs {                                       \
        int32_t pkey;                                                \
        void *address;                                               \
    };                                                               \
    __attribute__((constructor(102))) static void init_pkey_ctor() { \
        struct IA2PhdrSearchArgs args = {                            \
            .pkey = n + 1,                                           \
            .address = &init_pkey_ctor,                              \
        };                                                           \
        dl_iterate_phdr(protect_pages, &args);                       \
    }

#define INIT_RUNTIME(n_pkeys)                                                         \
    __attribute__((constructor(101))) static void init_runtime() {                    \
        for (int pkey = 0; pkey < n_pkeys; pkey++) {                                  \
            int allocated = pkey_alloc(0, 0);                                         \
            if (allocated != pkey + 1) {                                              \
                printf("Failed to allocate protection keys in the expected order\n"); \
                exit(0);                                                              \
            }                                                                         \
        }                                                                             \
    }
