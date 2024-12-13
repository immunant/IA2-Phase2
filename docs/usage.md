# Compartmentalization Guide

To compartmentalize a program, cross-compartment calls must go through call gate
wrappers which change PKRU, switch stacks, and scrub unused registers.
Compartments are the dynamic shared objects (DSOs) a program is comprised of and
they are either assigned one of 15 protection keys or default to the untrusted
protection key. This doc walks through how to compartmentalize a program using
our source rewriter.

## Running the Rewriter

The build process for a compartmentalized program is to first run the sources
through our source rewriter, then compile with any standard C toolchain with a
few additional flags. In order to run the rewriter a few preparation steps
must be taken.

### Defining Compartments

The rewriter needs to know which compartment each source file belongs to, and it
needs each compartment to be initialized in one of the source files. To specify
the compartment for each file, add a `-DPKEY=$PKEY` argument to the compiler,
where `$PKEY` is the compartment number for that source file.

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

### Capturing Compile Commands

The rewriter relies on a `compile_commands.json` file with information about the
compiler flags used to compile each source file. If your build system does not
directly support generating a compile commands file, you can use a tool like
`bear` to record the compiler invocations that your build system makes.

One limitiation is that the rewriter currently does not support having multiple
compile commands for the same source file. If your build system builds the same
source file multiple times, e.g. building for both a shared lib and a static
lib, you'll need to change your build such that it only builds the sources once.

Note that `libclangTooling`, which `ia2-rewriter` uses, is very finicky
about relative paths, so we suggest making all relative paths absolute.
Furthermore, `ia2-rewriter` itself sometimes uses paths as keys,
so we suggest making all paths canonical. We have a script,
[`canonicalize_compile_command_paths.py`](../tools/rewriter/canonicalize_compile_command_paths.py),
that does this automatically (doing its best to detect paths embedded in args).
`libclangTooling` also requires compilation databases to be named exactly
`compile_commands.json`, so this script assumes that as well,
and must be run in the directory of the `compile_commands.json`, like this:

```sh
(cd $build_directory && $ia2_dir/tools/rewriter/canonicalize_compile_command_paths.py)
```

### Running the Rewriter

Once you have added the code to initialize each compartment, and have generated
a `compile_commands.json` with the necessary `PKEY` values specified (via
`-DPKEY=N`), you can run the rewriter.

Instead of rewriting sources in-place, the rewriter creates a set of new,
intermediate source (and header) files. Since the rewriter only accepts a list
of `.c` source files, the set of intermediate headers that will be created is
controlled by the `--root-directory` and `--output-directory` command-line
flags. Any file from subdirectories of the root directory which is `#include`d
in an input `.c` is copied over to the output directory under the same
subdirectory. Any `#include`d file which is not under the root directory is
treated as a system header and does not get rewritten.

You'll also have to specify a prefix to use for the additional files that the
source rewriter generates.

For example, if you have two source files `src/foo.c` and `source/bar.c`, your
invocation of the rewriter would look like this:

```sh
$IA2_PATH/build/tools/rewriter/ia2-rewriter \
  --root-directory=$PROJ_ROOT \
  --output-directory=$OUT_DIR \
  --output-prefix=wrapper \
  $PROJ_ROOT/src/foo.c \
  $PROJ_ROOT/src/bar.c
```

Note that, like with capturing compile commands, we need to use absolute,
canonicalized paths for the list of files we specify to the rewriter.

In addition to putting the rewritten sources in `$OUT_DIR`, the rewriter will
generate some additional files in the directory where you run it. You can either
copy those files to a more appropriate location, or you can run the rewriter in
the appropriate output directory.

## Manual source changes

In some cases the rewriter will be unable to fully rewrite your code, and so
you'll need to make additional manual changes to your rewritten sources before
it will compile and run successfully.

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
-I $IA2_PATH/runtime/libia2/include
-I $IA2_PATH/runtime/partition-alloc/include
-Werror=incompatible-pointer-types
-Wl,--wrap=pthread_create
-Wl,--wrap=calloc
-Wl,--wrap=free
-Wl,--wrap=malloc
-Wl,--wrap=memalign
-Wl,--wrap=posix_memalign
-Wl,--wrap=pvalloc
-Wl,--wrap=realloc
-Wl,--wrap=valloc
-Wl,--wrap=malloc_usable_size
-Wl,--wrap=realpath
-Wl,--wrap=strdup
-Wl,--wrap=strndup
-Wl,--wrap=getcwd
-Wl,--wrap=asprintf
-Wl,--wrap=vasprintf
-pthread
-Wl,-z,now
-Wl,-z,relro
-Wl,-T$IA2_PATH/runtime/libia2/padding.ld

# For the DSO that initializes the runtime
-Wl,--wrap=main
-Wl,--dynamic-list=$IA2_PATH/runtime/libia2/dynsym.syms
-Wl,--export-dynamic
-L$IA2_PATH/build/runtime/libia2
-L$IA2_PATH/build/runtime/partition-alloc
-llibia2
-lpartition-alloc
-lcallgates
```

Also if the rewriter produces a linker args file for a given compartment (i.e. a
`.ld` file), you must include `-Wl,@/path/to/generated_linker_args_$PKEY.ld` when
linking that DSO.

## Using Thread Local Storage

When using TLS in a compartmentalized app, you'll need to run the `pad-tls` tool
on all shared objects in the app. This includes `libc.so` as libc both
initializes and makes use of TLS. If you're seeing compartment violations when
accessing TLS then you likely need to run `pad-tls` on the relevant. The tool
can be found at `$IA2_PATH/build/tools/pad-tls/pad-tls`. Note that the `ldd` and
`lddtree` tools can be used to list the DSO dependencies of your app.
