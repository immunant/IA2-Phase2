# Intent-capturing Annotations for Isolation and Assurance (IA<sup>2</sup> Phase 2)

This repo provides tools for compartmentalizing an application and its dependencies using Intel's memory protection keys (MPK) for userspace. The repo includes a tool that rewrites headers to create call gate wrappers as well as the runtime to initialize the protection keys for trusted compartments.

## Usage

### Defining compartments

First invoke the [`INIT_RUNTIME`](https://github.com/immunant/IA2-Phase2/blob/5cdb743d3a42e8df8e4d8cf61fb3551656001c73/libia2/include/ia2.h#L204) macro once in any binary or shared library to define the number of protection keys that need to be allocated. The argument passed to `INIT_RUNTIME` must be a number between 1 and 15. Then the [`IA2_COMPARTMENT`](https://github.com/immunant/IA2-Phase2/blob/4a3a0c8d2a2b1881e0e41c89db070db3da187f9e/libia2/include/ia2_compartment_init.inc#L3) `#define` is used to define trusted compartments at the shared object level. This can be the main executable ELF or any dynamically-linked shared libraries. Memory belonging to a trusted compartment is assigned one of the [15 protection keys](https://man7.org/linux/man-pages/man7/pkeys.7.html) and can only be accessed by the shared object itself. Objects that don't explicitly define a compartment are treated as untrusted by default.

To assign a protection key to a trusted compartment, insert `#define IA2_COMPARTMENT n` with an argument between 1-15 specifying the index of the protection key, and include `ia2_compartment_init.inc`:

```c
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>
```

This argument must differ from the other trusted compartments. Trusted compartments must also be aligned and padded properly by using the `padding.ld` script in `libia2/`. In CMake this is done automatically for executables built with `define_test` while libraries built with `define_shared_lib` must add `LINK_OPTS "-Wl,-T${libia2_BINARY_DIR}/padding.ld"`. To use in manual builds just include `-Wl,-T/path/to/padding.ld` in the final compilation step. Manual builds also require disabling lazy binding with `-Wl,-z,now`.

### Wrapping calls

Calls between compartments must have call gates to toggle the PKRU permissions at each transition. For direct calls, this is done by rewriting headers to generate the source for a wrapper that provides versions of every function with call gates. These wrappers are specific to both the wrapped library and caller. This means that the generated source must be compiled once per compartment that links against the wrapped library. Each caller's wrapper must define the `CALLER_PKEY` macro with the appropriate value for the caller.

#### From CMake

We provide a CMake rule to wrap a library or the main executable. This rule builds a wrapper and provides its dependency information to consumers of its outputs. Specifically, wrapper libs also depend on libia2 and have additional required compilation flags (-fno-omit-frame-pointer) for application code.

[Usage from CMake](https://github.com/immunant/IA2-Phase2/blob/main/cmake/define-ia2-wrapper.cmake#L10-L32) looks like this (wrapping `myunsafelib` which is used by your existing `my_prog` target):
```diff
+define_ia2_wrapper(
+    WRAPPER my_wrapper_target
+    WRAPPED_LIB myunsafelib-1.0
+    HEADERS myunsafelib.h myunsafelib_config.h
+    CALLER_PKEY 0
+)
+
 add_executable(my_prog main.c)
-target_link_libraries(my_prog PRIVATE myunsafelib-1.0)
+target_link_libraries(my_prog PRIVATE my_wrapper_target)
```

Wrapped libraries are treated as untrusted by default. If the library being wrapped defined a trusted compartment, `COMPARTMENT_PKEY n` must be specified in define_ia2_wrapper. Here `n` is the argument used in `IA2_COMPARTMENT` to define the compartment. If the caller is an untrusted compartment, set `CALLER_PKEY UNTRUSTED`. To create a wrapper for the main binary (i.e. if shared libraries call it directly) the `WRAP_MAIN` option must be specified.

#### Manual usage

See [this doc](https://github.com/immunant/IA2-Phase2/blob/main/docs/usage.md) for notes on manual usage.
