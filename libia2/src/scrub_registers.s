    # This file defines the feature specific scrub routines.
    #
    # This is implemented via the fairly standard fallthrough approach, and
    # does not bother to setup a frame since this is a leaf routine.
    # Note that while this follows general best practices for zero-idioms in
    # the generic case, it hasn't been performance tuned at all.  The avx512
    # version in particular could be optimized.
    .text
    .p2align 5
__libia2_scrub_registers_avx512:
    # zero avx512 vector registers (subsume xmm/ymm)
    vpxorq %zmm16, %zmm16, %zmm16
    vpxorq %zmm17, %zmm17, %zmm17
    vpxorq %zmm18, %zmm18, %zmm18
    vpxorq %zmm19, %zmm19, %zmm19
    vpxorq %zmm20, %zmm20, %zmm20
    vpxorq %zmm21, %zmm21, %zmm21
    vpxorq %zmm22, %zmm22, %zmm22
    vpxorq %zmm23, %zmm23, %zmm23
    vpxorq %zmm24, %zmm24, %zmm24
    vpxorq %zmm25, %zmm25, %zmm25
    vpxorq %zmm26, %zmm26, %zmm26
    vpxorq %zmm27, %zmm27, %zmm27
    vpxorq %zmm28, %zmm28, %zmm28
    vpxorq %zmm29, %zmm29, %zmm29
    vpxorq %zmm30, %zmm30, %zmm30
    vpxorq %zmm31, %zmm31, %zmm31
    # zero avx512 vector mask registers
    kxorb %k0, %k0, %k0
    kxorb %k1, %k1, %k1
    kxorb %k2, %k2, %k2
    kxorb %k3, %k3, %k3
    kxorb %k4, %k4, %k4
    kxorb %k5, %k5, %k5
    kxorb %k6, %k6, %k6
    kxorb %k7, %k7, %k7
    .p2align 5
__libia2_scrub_registers_sse:
    # Warning: Despite name, does NOT zero all ZMMs when AVX-512 enabled
    # must still handle YMM16-31 manually (e.g. use entry above)
    vzeroall
    .p2align 5
__libia2_scrub_registers_generic:
    xorq %rax, %rax
    xorq %rbx, %rbx
    xorq %rcx, %rcx
    xorq %rdx, %rdx
    xorq %rsi, %rsi
    xorq %rdi, %rdi
    xorq %rbp, %rbp
    xorq %r8, %r8
    xorq %r9, %r9
    xorq %r10, %r10
    xorq %r11, %r11
    xorq %r12, %r12
    xorq %r13, %r13
    xorq %r14, %r14
    xorq %r15, %r15
    # Clobber EFLAG condition codes.  Need an explicit instruction as
    # xorq does not effect AF.
    cmpq %rax, %rax
    retq

    .global __libia2_scrub_registers
    .p2align 5
__libia2_scrub_registers:
    # For the moment, we unconditionally assume you're running on a
    # target which has sse/avx/av2, but not avx512.  Someday, we should
    # support avx512
    jmp __libia2_scrub_registers_sse
    int3
