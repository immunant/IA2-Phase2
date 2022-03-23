# Source-to-Source Header Rewriting Design

In Phase 1 we relied on compiler instrumentation to insert call gates at
inter-compartment calls and for rewriting object allocations for shared values.
For Phase II we plan to instead use source rewriting and standard linking to
interpose between compartments, removing the requirement for a customized
compiler.

## Design Structure

Our goal is to compartmentalize library(s) at the dynamic linking interface
between shared libraries. These libraries generally use the C ABI and declare
their API using C headers. Our rewriter will take these header declarations as
input and produce a new, drop-in replacement library that exposes the same API
but includes compartment transitions when entering and exiting the original
library code. This replacement library will only contain call-gate wrappers for
each exported function and will dynamically link against the original library
for the actual implementations of the API.

By replacing the original library with our wrapper in the application build
system, users can be sure that they cannot inadvertently call functions in the
compartmentalized library directly, bypassing the compartment transition. This
also handles `dlopen` calls, as long as the applications `dlopen`s the wrapper
library rather than the original.

The rewriter will produce C files with inline call gates and stack transitions
that users can audit if desired and can build using their existing toolchain.
When we change types of function declarations (e.g. char* -> SharedCharPtr) to
indicate that a type needs to be allocated in the shared memory region, we will
need to produce a replacement header with these rewritten types for users to
include instead of the original library's header. Again, these headers will be
standard C and auditable after creation.

Using a C API is the lowest common denominator for FFI interop between
languages. Even C++ and Rust usually use the C ABI to interop with other
external libraries. We may need a different design if we want to support
compartmentalizing, e.g., pure Rust crates rather than external C libraries.

## Compartment Transitions

### Trusted -> Untrusted direct calls

Our wrapper library will contain wrappers for every exported function. Each
wrapper will save and clear callee-saved registers to the caller stack, save an
identifier or address to the stack to validate the return site, copy any
parameters on the stack to the callee stack, switch compartments, make a direct
call to the callee. On returning the wrapper will transition back to the
original compartment, check the stack cookie/return address to ensure that the
program returned to the corresponding wrapper, switch to the caller stack, and
return.

### Untrusted -> Trusted direct calls

Rare case. During wrapper creation we may be able to scan the target library for
unresolved symbols and provide reverse wrappers for these symbols.

### Trusted -> Untrusted indirect calls

Indirect calls to exported functions will go through our wrappers, as taking the
address of an exported function in the target library will give the address of
the wrapper. Private callbacks that aren't exported are trickier. We will
probably need to replace function pointer types in the headers with special
opaque structures that capture the function pointer and redirect it to a call
gate transition.

### Untrusted -> Trusted indirect calls

Same as trusted -> untrusted in the wrapper paradigm.