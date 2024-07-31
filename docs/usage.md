# Compartmentalization Guide

To compartmentalize a program, cross-compartment calls must go through call gate
wrappers which change PKRU, switch stacks, and scrub unused registers.
Compartments are the dynamic shared objects (DSOs) a program is comprised of and
they are either assigned one of 15 protection keys or default to the untrusted
protection key. This doc walks through how to compartmentalize a program using
our source rewriter.

## Build process

The build process for a compartmentalized program is to first run the sources
through our source rewriter (`ia2-rewriter`), then compile with any standard C toolchain with a
few additional flags. Instead of rewriting sources in-place, the rewriter
creates a set of new, intermediate source (and header) files. Since the rewriter
only accepts a list of `.c` source files, the set of intermediate headers that
will be created is controlled by the `--root-directory` and `--output-directory`
command-line flags. Any file from subdirectories of the root directory
which is `#include`d in an input `.c` is copied over to the output directory
under the same subdirectory. Any `#include`d file which is not under the root
directory is treated as a system header and does not get rewritten.

Additionally, the rewriter also takes an `--output-prefix` for naming
the build artifacts (call gates) it generates and a list of source files. Generally, you'll
want to generate and use a `compile_commands.json` to ensure the rewriter
preprocesses each source file with the same command-line arguments as when it is
compiled.

### `ia2-rewriter` Args

`ia2-rewriter` is a `libclangTooling` CLI, so it is very finicky about getting the arguments and paths exactly right.
For better reliability, always use absolute paths.
We'll try to explain the args in more detail here:

- `-p $build_dir`: `$build_dir` specifies the build directory
  containing a file named exactly `compile_commands.json`.
  The paths in `compile_commands.json` must be absolute paths for things to reliably work.

  If the compilation database or the paths inside of it are specified incorrectly,
  `libclangTooling` may guess its own incorrect compile command,
  leading to very confusing errors.
  `libclangTooling` does not make `-p` required,
  but you should specify it instead of relying on
  the fallback guessed compile command.

  Some tools, like `meson`, build with relative paths and thus create `compile_commands.json`s with relative paths.  To make these absolute paths, [`absolute_paths_compile_commands.py`](../tools/rewriter/absolute_paths_compile_commands.py).

  ```sh
  (cd $build_dir && $ia2_dir/tools/rewriter/absolute_paths_compile_commands.py)
  ```

  This does a best effort of detecting paths in arguments, such as `-I` args.

- `--root-directory $root_dir`: `$root_dir` is the root directory of the sources being rewritten.

- `--output-directory $output_dir`: `$output_dir` is the directory in which the rewritten sources are created.
  This directory must already exist, and existing files are overwritten.

- `--ouput-prefix $prefix`: `$prefix` is the file path prefix
  of the generated files (i.e. the call gate files).
  Thus, it is usually named `${output_dir}_call_gates`.

- `--extra-arg $arg`: Add an extra arg to the end of each compile command in `$build_dir/compile_commands.json`.
  The tests pass these extra args:

  ```sh
  --extra-arg=-isystem --extra-arg=$(llvm-config --libdir)/clang/*/include --extra-arg=-isystem --extra-arg=include-fixed
  ```

  If a different `llvm-config` was used to build IA2, use that one instead.

- `$sources`: The source files to rewrite.
  These are positional arguments that come after the abovementioned flag args.
  A single source can be rewritten at a time,
  but to rewrite a whole project, every source file in `$build_dir/compile_commands.json` should be specified here.

## Manual source changes

### Defining compartments

The compartments for each DSO are declared with macros in one of their
constituent source files. We also need to declare the number of pkeys used by
the runtime with another macro. Consider a main binary `main.c` which we want to
put in compartment 1 and a library `foo.c` in compartment 2.

```c
// main.c
#include <ia2.h>

INIT_RUNTIME(2); // This is the number of pkeys needed

// This must be defined before including the following line
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

// foo.c
#include <ia2.h>

#define IA2_COMPARTMENT 2
#include <ia2_compartment_init.inc>
```

Note that this must only be included in one source file per DSO.

### Sharing data

Some non-`const` statically-allocated data must be made accessible to all compartments to
avoid significant refactoring. In these cases, `IA2_SHARED_DATA` can be used to
place variables in a special ELF section that is explicitly accessible from all
compartments. Note that we assume that on-disk data (i.e. read-only variables)
is not sensitive, so this is only needed for some read-write variables.

### Signal handlers

Signal handlers require special call gate wrappers defined by the
`IA2_DEFINE_SIGACTION` or `IA2_DEFINE_SIGHANDLER` macros. To reference the
special call gate wrapper defined for a given function `foo`, use
`IA2_SIGHANDLER(foo)`.

### Interaction with system headers

Function pointers defined in system headers (or those outside
`--root-directory`) do not get rewritten as opaque types. Due to limitations in
the rewriter, this means that some annotations will be inserted and trigger
compiler errors due to type mismatches. For example, consider a compartment that
calls `qsort`. This is a libc function with the signature

```c
void qsort(void *ptr, size_t count, size_t size, int (*comp)(const void *, const void *));
```

Since it is declared in a system header, the fourth argument will remain a
function pointer instead of being rewritten as an opaque struct. However, the
rewriter currently sees the function pointer type passed in as the fourth
argument and rewrites it as

```diff
-qsort(ptr, count, size, cmp_fn)
+qsort(ptr, count, size, IA2_FN(cmp_fn))
```

To avoid this annotation, the `IA2_IGNORE` macro must be added around `cmp_fn` to
ensure it is not modified by the rewriter.

```c
qsort(ptr, count, size, IA2_IGNORE(cmp_fn))
```

The same logic applies if a system header defines a struct which contains
function pointers. Note that this temporary limitation of the rewriter will
always cause compiler errors at the sites that need to be changed.

### Function pointer annotations in macros

While the rewriter can rewrite object-like macros, automatically rewriting
function-like macros is currently not supported. Again, sites that need manual
changes will trigger compiler errors due to type mismatches. The following
macros are usually adequate to handle these cases and are documented in more
detail in [`ia2.h`](../runtime/libia2/include/ia2.h).

```c
IA2_FN_ADDR(func) // Get the address of the wrapper for `func`
IA2_ADDR(opaque) // Get the address of the wrapper pointed to by the struct `opaque`
IA2_AS_PTR(opaque) // Same as `IA2_ADDR` but may be used on the LHS for assignment
IA2_FN(func) // Reference the wrapper for `func` as an opaque struct
IA2_CALL(opaque, id) // Call the the wrapper that `opaque` points to
IA2_CAST(func, ty) // Get a struct of type `ty` pointing to `func`'s wrapper
```

## Building the callgate shim

Running the source rewriter produces a `.c` that will be used to build the
callgate shim library. To compile it, run

```sh
$CC -shared -fPIC -Wl,-z,now callgate_wrapper.c -I /path/to/ia2/runtime/libia2/include -o libcallgates.so
```

## Compiling and linking the program

In addition to the flags normally used to build the sources, the following flags
are also required:

```sh
# For all DSOs
-fPIC
-DPKEY=$PKEY
-DIA2_ENABLE=1
-include /path/to/generated_output_header.h
-Werror=incompatible-pointer-types
-Wl,--wrap=pthread_create
-pthread
-Wl,-z,now
-Wl,-z,relro
-Wl,-T$IA2_PATH/runtime/libia2/padding.ld

# For the DSO that initializes the runtime
-Wl,--wrap=main
-Wl,--dynamic-list=$IA2_PATH/runtime/libia2/dynsym.syms
-Wl,--export-dynamic
```

Also if the rewriter produces a linker args file for a given compartment (i.e. a
`.ld` file), you must include `-Wl,@/path/to/generated_linker_args_$PKEY.ld` when
linking that DSO.
