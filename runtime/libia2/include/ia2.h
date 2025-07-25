#pragma once

#ifndef IA2_ENABLE
#define IA2_ENABLE 0
#endif

// This include must come first so we define _GNU_SOURCE before including
// standard headers. ia2_internal.h requires GNU-specific headers.
#if IA2_ENABLE
#include "ia2_internal.h"
#endif

#include <errno.h>
#include <stdint.h>
#include <unistd.h>

/// Do not wrap functions or function pointers in the following code.
///
/// This must be paired with a matching IA2_END_NO_WRAP that pops the
/// annotation.
///
/// Some functions, e.g. utility functions that don't touch global state, should
/// not be wrapped and should always run in the caller compartment. If we wrap
/// these functions they may act as simple accessors for another compartment's
/// private data. For example, consider an vector_pop() function that pops an
/// element from a vector. If this function is declared in compartment A and
/// used from compartment B, it allows compartment B to pop any element from
/// compartment A's private vectors.
///
/// Any functions declared between this macro and IA2_END_NO_WRAP will not be
/// wrapped by the rewriter and any calls to these functions and function
/// pointers will execute in the caller's compartment.
#define IA2_BEGIN_NO_WRAP                                                      \
  _Pragma(                                                                     \
      "clang attribute push(__attribute__((annotate(\"ia2_skip_wrap\"))), apply_to = hasType(functionType))");

#define IA2_END_NO_WRAP _Pragma("clang attribute pop");

/// Mark the annotated function as a pre-condition function for `target_func`.
///
/// It will be called with the first 6 arguments of `target_func` before `target_func` is called.
#define IA2_PRE_CONDITION_FOR(target_func) __attribute__((annotate("ia2_pre_condition:" #target_func)))

/// Mark the annotated function as a post-condition function for `target_func`.
///
/// It will be called with the first 6 arguments of `target_func` after `target_func` is called.
#define IA2_POST_CONDITION_FOR(target_func) __attribute__((annotate("ia2_post_condition:" #target_func)))

/// Mark the annotated function as a constructor for the type `T`,
/// where the function has the signature `R f(T* this, ...)`.
/// The `...` aren't varargs here; it just means that
/// there can be any number of extra args after the `T*`.
///
/// This function should initialize the `T*`
/// and marks the start of its lifetime as a
/// registered and valid ptr of type `T`.
///
/// Note that this is not the same as `__attribute__((constructor))`.
#define IA2_CONSTRUCTOR __attribute__((annotate("ia2_constructor")))

/// Mark the annotated function as a destructor for the type `T`,
/// where the function has the signature `void f(T* this)`.
///
/// This function should unitialize the `T*`
/// and marks the end of its lifetime as a
/// registered and valid ptr of type `T`.
///
/// Note that this is not the same as `__attribute__((destructor))`.
#define IA2_DESTRUCTOR __attribute__((annotate("ia2_destructor")))

#if !IA2_ENABLE
#define IA2_DEFINE_WRAPPER(func)
#define IA2_SIGHANDLER(func) func
#define IA2_DEFINE_SIGACTION(function, pkey)
#define IA2_DEFINE_SIGHANDLER(function, pkey)
#define INIT_RUNTIME(n)
#define IA2_SHARED_DATA __attribute__((section("ia2_shared_data")))
#define ASSERT_PKRU(pkru)
#define IA2_IGNORE(x) x
#define IA2_FN_ADDR(func) func
#define IA2_ADDR(opaque) (void *)opaque
#define IA2_AS_PTR(opaque) opaque
#define IA2_FN(func) func
#define IA2_CALL(opaque, id, ...) opaque(__VA_ARGS__)
#define IA2_CAST(func, ty) (ty) (void *) func
#else
#define IA2_DEFINE_WRAPPER(func) IA2_DEFINE_WRAPPER_##func
#define IA2_SIGHANDLER(func) ia2_sighandler_##func
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
#ifndef IA2_DEBUG
#define ASSERT_PKRU(pkru)
#elif defined(__x86_64__)
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
#else
#warning "ASSERT_PKRU is unimplemented on this target"
#define ASSERT_PKRU(pkru)
#endif

#define IA2_IGNORE(x) x

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
#define IA2_CALL(opaque, id, ...) _IA2_CALL(opaque, id, PKEY, ##__VA_ARGS__)

/// Get an IA2 opaque function pointer of type `ty` for the wrapper version of `func`
///
/// This is the same as IA2_FN but with the type determined by the macro
/// parameter. Note that it is the user's responsibility to ensure that `ty` and
/// the type of `IA2_FN(func)` are ABI-compatible since no extra type-checking is
/// done.
#define IA2_CAST(func, ty) (ty) { (void *)IA2_FN_ADDR(func) }
#endif // !IA2_ENABLE

#define IA2_MAX_COMPARTMENTS 16

/// Convert a compartment pkey to a PKRU register value
#define PKRU(pkey) (~((3U << (2 * pkey)) | 3))

#ifdef __cplusplus
extern "C" {
#endif

/// Returns the arch-specific compartment tag value.
///
/// On x86-64 this is the PKRU while on aarch64 it's the 4-bit tag in the upper
/// byte of x18.
size_t ia2_get_tag();

/// Returns the current compartment number
size_t ia2_get_compartment();

/// Registers the DSOs associated with a given protected compartment. For use in ia2_main.
///
/// `lib` must be the filename of a ` DSO or "main" for the main executable.`extra_libraries` must
/// be a semicolon-separated list of libraries or NULL. The pointers must remain valid after
/// ia2_main returns. This function should not be called for compartment 0 since it is not a
/// protected compartment.
void ia2_register_compartment(const char *lib, int compartment, const char *extra_libraries);

/// The prototype for the user-defined config function which will be called before main.
///
/// This function must be defined in the main executable and should primarily be used to call
/// ia2_register_compartment for each protected compartment.
extern void ia2_main(void);

#ifdef __cplusplus
}
#endif
