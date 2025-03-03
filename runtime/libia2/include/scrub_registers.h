#pragma once

#if defined(__x86_64__)
// This file defines the feature specific scrub routines.
//
// This is implemented via the fairly standard fallthrough approach, and
// does not bother to setup a frame since this is a leaf routine.
// Note that while this follows general best practices for zero-idioms in
// the generic case, it hasn't been performance tuned at all.  The avx512
// version in particular could be optimized.
asm(".text\n"
    ".p2align 5\n"
    "__libia2_scrub_registers_avx512:\n"
    "vpxorq %zmm16, %zmm16, %zmm16\n"
    "vpxorq %zmm17, %zmm17, %zmm17\n"
    "vpxorq %zmm18, %zmm18, %zmm18\n"
    "vpxorq %zmm19, %zmm19, %zmm19\n"
    "vpxorq %zmm20, %zmm20, %zmm20\n"
    "vpxorq %zmm21, %zmm21, %zmm21\n"
    "vpxorq %zmm22, %zmm22, %zmm22\n"
    "vpxorq %zmm23, %zmm23, %zmm23\n"
    "vpxorq %zmm24, %zmm24, %zmm24\n"
    "vpxorq %zmm25, %zmm25, %zmm25\n"
    "vpxorq %zmm26, %zmm26, %zmm26\n"
    "vpxorq %zmm27, %zmm27, %zmm27\n"
    "vpxorq %zmm28, %zmm28, %zmm28\n"
    "vpxorq %zmm29, %zmm29, %zmm29\n"
    "vpxorq %zmm30, %zmm30, %zmm30\n"
    "vpxorq %zmm31, %zmm31, %zmm31\n"
    "kxorb %k0, %k0, %k0\n"
    "kxorb %k1, %k1, %k1\n"
    "kxorb %k2, %k2, %k2\n"
    "kxorb %k3, %k3, %k3\n"
    "kxorb %k4, %k4, %k4\n"
    "kxorb %k5, %k5, %k5\n"
    "kxorb %k6, %k6, %k6\n"
    "kxorb %k7, %k7, %k7\n"
    ".p2align 5\n"
    "__libia2_scrub_registers_sse:\n"
    /* Warning: Despite name, does NOT zero all ZMMs when AVX-512 enabled
       must still handle YMM16-31 manually (e.g. use entry above) */
    "vzeroall\n"
    ".p2align 5\n"
    "__libia2_scrub_registers_generic:\n"
    "xorq %rax, %rax\n"
    "xorq %rbx, %rbx\n"
    "xorq %rcx, %rcx\n"
    "xorq %rdx, %rdx\n"
    "xorq %rsi, %rsi\n"
    "xorq %rdi, %rdi\n"
    /* TODO: Figure out what to do about the frame pointer */
    /* "xorq %rbp, %rbp\n" */
    "xorq %r8, %r8\n"
    "xorq %r9, %r9\n"
    "xorq %r10, %r10\n"
    "xorq %r11, %r11\n"
    "xorq %r12, %r12\n"
    "xorq %r13, %r13\n"
    "xorq %r14, %r14\n"
    "xorq %r15, %r15\n"
    /* Clobber EFLAG condition codes.  Need an explicit instruction as
       xorq does not effect AF. */
    "cmpq %rax, %rax\n"
    "retq\n"
    ".p2align 5\n"
    /* We intentionally omit emitting a symbol for this label since we treat it
       like a static function (avoid calling it through the PLT) in the wrapper
       sources and binaries that invoke the FNPTR macros */
    "__libia2_scrub_registers:\n"
    /* For the moment, we unconditionally assume you're running on a
       target which has sse/avx/av2, but not avx512.  Someday, we should
       support avx512 */
    "jmp __libia2_scrub_registers_sse\n"
    "int3\n"
    ".previous\n");
#elif defined(__aarch64__)
#warning "__libia2_scrub_registers for aarch64 is not complete yet"
asm(".text\n"
    ".p2align 5\n"
    "__libia2_scrub_registers:\n"
    "eor x0, x0, x0\n"
    "eor x1, x1, x1\n"
    "eor x2, x2, x2\n"
    "eor x3, x3, x3\n"
    "eor x4, x4, x4\n"
    "eor x5, x5, x5\n"
    "eor x6, x6, x6\n"
    "eor x7, x7, x7\n"
    "eor x8, x8, x8\n"
    "eor x9, x9, x9\n"
    "eor x10, x10, x10\n"
    "eor x11, x11, x11\n"
    "eor x12, x12, x12\n"
    "eor x13, x13, x13\n"
    "eor x14, x14, x14\n"
    "eor x15, x15, x15\n"
    "eor x16, x16, x16\n"
    "eor x17, x17, x17\n"
    // x18
    "eor x19, x19, x19\n"
    "eor x20, x20, x20\n"
    "eor x21, x21, x21\n"
    "eor x22, x22, x22\n"
    "eor x23, x23, x23\n"
    "eor x24, x24, x24\n"
    "eor x25, x25, x25\n"
    "eor x26, x26, x26\n"
    "eor x27, x27, x27\n"
    "eor x28, x28, x28\n"
    "ret\n"
    // x29, x30
    ".previous\n");
#endif
