#if __x86_64__
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
#elif __aarch64__
#warning "__libia2_scrub_registers is not implemented for aarch64 yet"
#endif
