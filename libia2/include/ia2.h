#pragma once

// This include must come first so we define _GNU_SOURCE before including
// standard headers. ia2_internal.h requires GNU-specific headers.
#include "ia2_internal.h"

#include <stdint.h>

/// Create a wrapped signal handler for `sa_sigaction`
///
/// Wraps the given function with `pkey`, creating a handler for use with
/// sigaction. The given function should have the type signature: `void (int,
/// siginfo_t*, void*)`. This handler may be arbitrarily called by code in other
/// compartments, so must be safe to execute at any time.
///
/// Creates a new function with `ia2_sighandler_` prepended to the given
/// function name which should be registered with sigaction().
#define IA2_DEFINE_SIGACTION(function, pkey)                                   \
  void ia2_sighandler_##function(int, siginfo_t *, void *);                    \
  _IA2_DEFINE_SIGNAL_HANDLER(function, pkey)

/// Create a wrapped signal handler for `sa_handler`
///
/// Wraps the given function with `pkey`, creating a handler for use with
/// sigaction. The given function should have the type signature: `void (int)`.
/// This handler may be arbitrarily called by code in other compartments, so
/// must be safe to execute at any time.
///
/// Creates a new function with `ia2_sighandler_` prepended to the given
/// function name which should be registered with sigaction().
#define IA2_DEFINE_SIGHANDLER(function, pkey)                                  \
  void ia2_sighandler_##function(int);                                         \
  _IA2_DEFINE_SIGNAL_HANDLER(function, pkey)

/// Initialize the IA2 runtime, must only be invoked once per in a process
///
/// This macro inserts the necessary code to initialize the IA2 runtime as a
/// constructor function. This macro should generally be invoked once in the
/// main binary source code.
#define INIT_RUNTIME(n) _IA2_INIT_RUNTIME(n)

/// Attribute for read-write variables that should be accessible from any
/// compartment.
#define IA2_SHARED_DATA __attribute__((section("ia2_shared_data")))

/// Assembly to check and assert that `pkru` matches the current PKRU register
/// value
///
/// `pkru` should be the register value, not the compartment pkey.
#ifndef LIBIA2_DEBUG
#define ASSERT_PKRU(pkru)
#else
/* clang-format off */
#define ASSERT_PKRU(pkru)                       \
  "movq %rcx, %r10\n"                           \
  "movq %rdx, %r11\n"                           \
  "xorl %ecx, %ecx\n"                           \
  "rdpkru\n"                                    \
  "cmpl $" #pkru ", %eax\n"                     \
  "je 1f\n"                                     \
  "ud2\n"                                       \
"1:\n"                                          \
  "movq %r11, %rdx\n"                           \
  "movq %r10, %rcx\n"
/* clang-format on */
#endif

#define IA2_IGNORE(x) x

#if IA2_PREREWRITER
#define IA2_FN_ADDR(func) func
#define IA2_ADDR(opaque) (void *)opaque
#define IA2_AS_PTR(opaque) opaque
#define IA2_FN(func) func
#define IA2_CALL(opaque, id) opaque
#define IA2_CAST(func, ty) (ty) (void *) func
#else
/// Get the address of the wrapper function for `func`
#define IA2_FN_ADDR(func) (typeof(&func))(&__ia2_##func)

/// Get the raw function pointer out of an opaque IA2 function pointer
///
/// This macro should only be used for pointer comparison, do not store, cast,
/// or call this pointer.
#define IA2_ADDR(opaque) (void *)((opaque).ptr)

/// Get the raw function pointer out of an IA2 opaque function pointer without
/// casting to void *
///
/// This is useful for assignments with ABI-compatible pointers since IA2_ADDR
/// cannot be used on the lhs.
#define IA2_AS_PTR(opaque) (opaque).ptr

/// Get an IA2 opaque function pointer for the wrapped version of `func`
#define IA2_FN(func)                                                           \
  (typeof(__ia2_##func)) { (void *)&__ia2_##func }

/// Call an IA2 opaque function pointer, which should be in target compartment
/// `id`
#define IA2_CALL(opaque, id) _IA2_CALL(opaque, id, PKEY)

/// Get an IA2 opaque function pointer of type `ty` for the wrapper version of `func`
///
/// This is the same as IA2_FN but with the type determined by the macro
/// parameter. Note that it is the user's responsibility to ensure that `ty` and
/// the type of `IA2_FN(func)` are ABI-compatible since no extra type-checking is
/// done.
#define IA2_CAST(func, ty) (ty) { (void *)IA2_FN_ADDR(func) }
#endif

/// Convert a compartment pkey to a PKRU register value
#define PKRU(pkey) (~((3U << (2 * pkey)) | 3))

#ifdef __cplusplus
extern "C" {
#endif

/// Returns the raw PKRU register value
uint32_t ia2_get_pkru();

/// Returns the current compartment pkey
size_t ia2_get_pkey();

#ifdef __cplusplus
}
#endif
