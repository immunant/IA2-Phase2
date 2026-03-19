#if defined(IA2_LIBC_COMPARTMENT) && IA2_LIBC_COMPARTMENT

// -----------------------------------------------------------------------------
// Exit call gates
// -----------------------------------------------------------------------------
//
// The x86_64 implementation of exit call gates is provided in inline assembly
// to avoid stack corruption bugs that occur when modifying %rsp in C code.
//
// The __wrap___cxa_finalize function performs PKRU/stack transitions when
// entering the exit compartment (pkey IA2_LIBC_COMPARTMENT, where libc/ld.so
// live).
//
// The transition follows the same pattern as the rewriter-generated call gates
// in GenCallAsm.cpp:
//   1. Save callee-saved registers and caller state on the caller's stack
//   2. Switch PKRU to the exit compartment value (pkey 0 + libc pkey)
//   3. Load exit stack pointer directly from TLS via %fs:@GOTTPOFF
//      (no function call needed — avoids stack access while caller's stack
//      may be inaccessible under the new PKRU)
//   4. Switch to exit stack, call __real___cxa_finalize
//   5. Restore caller stack and PKRU, return
//
// Control-flow note: we do not copy the wrapper's caller return address onto
// the exit stack. __real___cxa_finalize returns to this wrapper while %rsp is
// on the exit stack, then we restore %rsp from r14 and `ret` pops the original
// return address from the caller stack frame.
//
// For other architectures, an assembly implementation must be provided.

#include "ia2.h"
#include "ia2_internal.h"

#if defined(__x86_64__)

__asm__(
    ".text\n"
    ".globl __wrap___cxa_finalize\n"
    ".type __wrap___cxa_finalize,@function\n"
"__wrap___cxa_finalize:\n"
    // Save callee-saved registers on caller's stack
    "pushq %rbp\n"
    "movq %rsp, %rbp\n"
    "pushq %rbx\n"
    "pushq %r12\n"
    "pushq %r13\n"
    "pushq %r14\n"
    "pushq %r15\n"

    "movq %rdi, %r12\n"                // preserve dso_handle
    "movq %rsp, %r14\n"                // save caller stack pointer

    // Read and save caller's PKRU
    "xorl %ecx, %ecx\n"
    "xorl %edx, %edx\n"
    "rdpkru\n"
    "movl %eax, %r13d\n"               // save caller PKRU

    // Switch to exit compartment PKRU (pkey 0 + pkey IA2_LIBC_COMPARTMENT).
    // After this the caller's stack (pkey 2+) may be inaccessible, but we
    // only perform register ops and one specific TLS read (ia2_stackptr_<libc>)
    // until the stack switch.
    "xorl %ecx, %ecx\n"
    "xorl %edx, %edx\n"
    "mov_pkru_eax " IA2_STR(IA2_LIBC_COMPARTMENT) "\n"
    "wrpkru\n"

    // Load exit stack pointer directly from TLS — same pattern as the
    // rewriter-generated call gates (emit_load_sp_offset / emit_switch_stacks
    // in GenCallAsm.cpp) and __wrap_main in main.c.
    // Safety invariant: this ia2_stackptr_<libc> @GOTTPOFF lookup and read
    // must remain accessible right after wrpkru. This does not imply arbitrary
    // TLS reads are safe in this window.
    "mov ia2_stackptr_" IA2_STR(IA2_LIBC_COMPARTMENT) "@GOTTPOFF(%rip), %r11\n"
    "movq %fs:(%r11), %r10\n"          // load exit stack pointer
    // Switch to exit stack. No synthetic return address push is needed.
    "movq %r10, %rsp\n"

    // Call __real___cxa_finalize with preserved dso_handle
    "movq %r12, %rdi\n"
    "subq $8, %rsp\n"                  // maintain 16-byte alignment
    "call __real___cxa_finalize@PLT\n"
    "addq $8, %rsp\n"

    // Restore caller stack pointer (register move — no memory access)
    "movq %r14, %rsp\n"

    // Restore caller's PKRU
    "movl %r13d, %eax\n"
    "xorl %ecx, %ecx\n"
    "xorl %edx, %edx\n"
    "wrpkru\n"

    // ASSERT_PKRU only accepts immediate literals; r13d needs a manual
    // rdpkru/compare check.
#ifdef IA2_DEBUG
    "movq %rcx, %r10\n"                // save rcx (clobbered by rdpkru)
    "rdpkru\n"
    "cmpl %r13d, %eax\n"               // compare requested PKRU vs actual
    "je 2f\n"
    "ud2\n"                            // trap if mismatch
"2:\n"
    "movq %r10, %rcx\n"                // restore rcx
#endif

    // Restore callee-saved registers and return
    "popq %r15\n"
    "popq %r14\n"
    "popq %r13\n"
    "popq %r12\n"
    "popq %rbx\n"
    "popq %rbp\n"
    "ret\n"

    ".size __wrap___cxa_finalize, .-__wrap___cxa_finalize\n"
);

#else
#error "IA2 exit callgates are only supported on x86_64 for now."
#endif // defined(__x86_64__)

#endif // IA2_LIBC_COMPARTMENT
