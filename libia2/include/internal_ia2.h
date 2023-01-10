#ifndef INTERNAL_IA2_H
#define INTERNAL_IA2_H

#include "scrub_registers.h"

#define STR(s) #s
#define XSTR(s) STR(s)
#define EXPAND(s) s

#ifdef LIBIA2_INSECURE
#define IA2_WRPKRU
#else
#define IA2_WRPKRU "wrpkru\n"
#endif

#define STACK(n) _STACK(n)
#define _STACK(n) STACK_##n
#define STACK_0 "0"
#define STACK_1 "8"
#define STACK_2 "16"
#define STACK_3 "24"
#define STACK_4 "32"
#define STACK_5 "40"
#define STACK_6 "48"
#define STACK_7 "56"
#define STACK_8 "64"
#define STACK_9 "72"
#define STACK_10 "80"
#define STACK_11 "88"
#define STACK_12 "96"
#define STACK_13 "104"
#define STACK_14 "112"
#define STACK_15 "120"

// This emits the 5 bytes correponding to the movl $PKRU, %eax instruction
asm(".macro mov_pkru_eax pkey\n"
    ".byte 0xb8\n"
    ".long ~((3 << (2 * \\pkey)) | 3)\n"
    ".endm");

// This emits the 5 bytes correponding to the movl $PKRU, %eax instruction
asm(".macro mov_mixed_pkru_eax pkey0, pkey1\n"
    ".byte 0xb8\n"
    ".long ~((3 << (2 * \\pkey0)) | (3 << (2 * \\pkey1)) | 3)\n"
    ".endm");

#endif
