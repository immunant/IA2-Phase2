#pragma once
#include "pkey_init.h"
#include <stdio.h>

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
#define GATE_UNTRUSTED                      \
    "movq %rcx, %r10\n"                     \
    "movq %rdx, %r11\n"                     \
    "movl $" XSTR(PKRU_UNTRUSTED) ", %eax\n" \
    "xorl %ecx, %ecx\n"                     \
    "xorl %edx, %edx\n"                     \
    "wrpkru\n"                              \
    "movq %r10, %rcx\n"                     \
    "movq %r11, %rdx\n"

// TODO: Add static assert on pkey index if possible
// Set PKRU based on pkey_idx using rax, r10 and r11 as scratch registers.
#define GATE_TRUSTED(pkey)                              \
    "movq %rcx, %r10\n"                                 \
    "movq %rdx, %r11\n"                                 \
    "movq IA2_INIT_DATA@GOTPCREL(%rip), %rax\n"         \
    "movl $" #pkey ", %ecx\n"                            \
    "movl (%rax,%rcx,4), %eax\n"                        \
    "cmpl $" XSTR(PKEY_UNINITIALIZED) ", %eax\n"         \
    "jne 0f\n"                                          \
    "call exit@plt\n"                                   \
    "0:\n"                                              \
    /* Calculate the new PKRU from the key index */     \
    /* new_pkru = ~((3 << (2 * pkey_idx)) | 3); */      \
    "addl %eax, %eax\n"                                 \
    "movl %eax, %ecx\n"                                 \
    "movl $3, %edx\n"                                    \
    "shll %cl, %edx\n"                                  \
    "movl %edx, %eax\n"                                 \
    "addl $3, %eax\n"                                    \
    "notl %eax\n"                                       \
    "xorl %ecx, %ecx\n"                                 \
    "xorl %edx, %edx\n"                                 \
    "wrpkru\n"                                          \
    "movq %r10, %rcx\n"                                 \
    "movq %r11, %rdx\n"

#ifdef LIBIA2_INSECURE
#define GATE(x)
#define DISABLE_PKRU
#else
#define GATE(pkey) _GATE(pkey)
// FIXME: Remove this after removing INIT_DATA (see issue #66).
#define DISABLE_PKRU    \
    "movq %rcx, %r10\n"                                 \
    "movq %rdx, %r11\n"                                 \
    "xorl %eax, %eax\n"                                 \
    "xorl %ecx, %ecx\n"                                 \
    "xorl %edx, %edx\n"                                 \
    "wrpkru\n"                                          \
    "movq %r10, %rcx\n"                                 \
    "movq %r11, %rdx\n"

#endif

#define _GATE(pkey) GATE_##pkey
#define GATE_0 GATE_TRUSTED(0)
#define GATE_1 GATE_TRUSTED(1)
#define GATE_2 GATE_TRUSTED(2)
#define GATE_3 GATE_TRUSTED(3)
#define GATE_4 GATE_TRUSTED(4)
#define GATE_5 GATE_TRUSTED(5)
#define GATE_6 GATE_TRUSTED(6)
#define GATE_7 GATE_TRUSTED(7)
#define GATE_8 GATE_TRUSTED(8)
#define GATE_9 GATE_TRUSTED(9)
#define GATE_10 GATE_TRUSTED(10)
#define GATE_11 GATE_TRUSTED(11)
#define GATE_12 GATE_TRUSTED(12)
#define GATE_13 GATE_TRUSTED(13)
#define GATE_14 GATE_TRUSTED(14)

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
    DISABLE_PKRU                                                               \
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
#define INIT_COMPARTMENT(n)                                     \
    NEW_SECTION(".fini_padding");                               \
    NEW_SECTION(".rela.plt_padding");                           \
    NEW_SECTION(".eh_frame_padding");                           \
    NEW_SECTION(".bss_padding");                                \
    __attribute__((constructor)) static void init_pkey_ctor() { \
        initialize_compartment(n, &init_pkey_ctor);             \
    }
