# Compartmentalizing `dav1d`

This is a thorough overview of the steps taken to compartmentalize the AV1 decoder `dav1d`.

The necessary changes can be found in [`immunant/dav1d/ia2`](https://github.com/immunant/dav1d/tree/ia2),
which was branched off from [`videolan/dav1d/master`](https://code.videolan.org/videolan/dav1d)
to [`immunant/dav1d/master`](https://github.com/immunant/dav1d/tree/master),
and the changes can be seen in [`master..ia2`](https://github.com/immunant/dav1d/compare/master...ia2).

## Building IA2

The IA2 tools that we need to compartmentalize and run `dav1d` are:

| Binary Name             | `ninja` Target Name       |
| ----------------------- | ------------------------- |
| `ia2-rewriter`          | `rewriter`                |
| `pad-tls`               | `pad-tls`                 |
| `libpartition-alloc.so` | `partition-alloc-padding` |
| `liblibia2.a`           | `libia2`                  |

As described in [`build_instructions`](./build_instructions.md),
we can build IA2 by running

```sh
mkdir build
cd build
cmake .. \
    -DClang_DIR=$(llvm-config --cmakedir)/../clang \
    -DLLVM_DIR=$(llvm-config --cmakedir) \
    -DLLVM_EXTERNAL_LIT=$(which lit) \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DIA2_DEBUG_LOG=True
```

We can build for another `-DCMAKE_BUILD_TYPE` as well,
but a debug build like `Debug` or `RelWithDebInfo` is recommended
in case tools crash and need to be debugged.

`-DIA2_DEBUG_LOG=True` also helps with debugging the compartmentalized `dav1d`, too.

## Choosing Compartments

For simplicity, simple compartments were chosen.
One compartment was chosen to be the `dav1d` CLI (mostly in `tools/`)
and a second to be the `libdav1d` library (mostly in `src/`).
The compartment boundary for this is conveniently very clear.
All cross-compartment boundaries are marked `DAV1D_API`
and all cross-compartment types are in `include/`, not `src/`.

## Numbering Compartments

Compartments and their pkeys (protection keys) cannot be chosen arbitrarily.

* Compartments must be numbered consecutively.
* Compartment `0` is always the untrusted/shared compartment, which exists implicitly,
so it doesn't need to be numbered (with `#define IA2_COMPARTMENT`).
* Comparment `1` must be the main compartment that defines `int main`.

Thus, the `dav1d` CLI main compartment was numbered `1`
and the `libdav1d` library compartment was numbered `2`.

In determining the number of compartments as declared with `INIT_RUNTIME`,
the `0` compartment is not counted, so this compartmentalization of `dav1d` has 2 compartments.

For these compartments, there are clear primary translation units to add the IA2 declarations to.
For the `dav1d` CLI compartment, it's the file with `int main`,
so `tools/dav1d.c`.  For the `libdav1d` compartment,
it's the file with most of the `DAV1D_API` definitions, so `src/lib.c`.

Since `tools/dav1d.c` is the main compartment,
it gets this IA2 declaration that calls `INIT_RUNTIME`:

```c
#include <ia2.h>
INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>
```

For the other compartment (`src/lib.c`),
it gets this IA2 declaration:

```c
#include <ia2.h>
#define IA2_COMPARTMENT 2
#include <ia2_compartment_init.inc>
```

## Rewriting

Next, we must run `ia2-rewriter` on `dav1d` to get the rewritten sources.
This is tricky, as `dav1d` is built with `meson`,
while the existing compartmentalized builds in the IA2 repo use `cmake`.
There are a lot of undocumented steps in that,
and things that `meson` does differently, so they're documented here:

### Generating a `compile_commands.json`

`ia2-rewriter` is a `libclangTooling` tool,
so it operates on a `compile_commands.json` compilation database.
`meson`, like `cmake`, can generate a `compile_commands.json`,
and does so by default when running `meson setup`.

#### Absolute Paths and `canonicalize_compile_command_paths.py`

However, it generates a `compile_commands.json` with relative paths.
`libclangTooling` hates relative paths and usually does the incorrect thing with them.
To get around this, we added a simple script,
[`canonicalize_compile_command_paths.py`](../tools/rewriter/canonicalize_compile_command_paths.py),
which rewrites a `compile_commands.json`'s paths to be absolute (and canonical/fully resolved).
This includes paths that can be detected within arguments,
though this is detected as a best effort.
Because `libclangTooling` requires a compilation database
to be named exactly `compile_commands.json`,
`canonicalize_compile_command_paths.py` assumes this and operates on
a `compile_commands.json` in the current working directory.

#### Other Flags

`ia2-rewriter` also requires some other flags to be set.
Some of these can be set by `libclangTooling`'s `--extra-arg` argument,
those apply to all of the compile commands at once,
so when we want different arguments to be added to different compile commands,
we need to add these to the build system, to `meson`,
in order to have them show up in the `compile_commands.json`.

The main argument that is like this is `-DPKEY`.
Unlike `#define IA2_COMPARTMENT` and `INIT_RUNTIME`,
`PKEY` must be defined for every translation unit.
This is easiest to add in the build system,
so that when we run `meson setup`, the `compile_commands.json` generated
contains the correct `-DPKEY`s that `ia2-rewriter` will see.

There are also other args that we need that are the same for all compile commands.
So these could be specified with `--extra-arg`,
but as we're already modifying the `meson.build`s, it's simpler to just modify them there.

First, there are required args:

* `-DIA2_ENABLE=1`
* `'-I' + join_paths(ia2_path, 'runtime/libia2/include')`

Then there are args for debugging:

* `-DIA2_DEBUG=1` (for debug assertions)
* `-DIA2_DEBUG_LOG=1` (for verbose logging)

Then there are overrides:

First just disabling some warnings/errors that `dav1d` enabled but IA2's runtime doesn't follow
(so this would be different for another project):

* `-Wno-strict-prototypes`
* `-Wno-missing-prototypes`
* `-Wno-unused-function`
* `-Wno-unknown-warning-option`

And then we also have to enable GNU C extensions:

* `-std=gnu99`

`dav1d` uses `-std=c99`, so we use `-std=gnu99`.
IA2 depends on some GNU C extensions for some assembly stuff and for statement expressions.

Documenting this properly is tracked in [#389](https://github.com/immunant/IA2-Phase2/issues/389),
though at least this document now documents it somewhat (for `dav1d`).

## Rewriting

Now that we've generated a `compile_commands.json` that works with `ia2-rewriter`,
we can finally run `ia2-rewriter`.  This is a bit complex,
as `ia2-rewriter` writes the rewritten files to new files in a given directory,
and integrating these new files into the build system is tricky.
Thus, for `dav1d`, we cloned the repo (as `dav1d-ia2`),
generated the `compile_commands.json` in the original `dav1d` repo,
ran `ia2-rewriter` on this `compile_commands.json`
with it writing to `dav1d-ia2`, overwriting the files there.
This is a bit confusing, but you can see how it's done in
[`rewrite.py`](https://github.com/immunant/dav1d/blob/ia2/rewrite.py),
as well as all of these other scripted changes.

To actually run `ia2-rewriter`, it is important to know that
it must be run on all of the sources you want to rewrite all at once.
Don't run it file by file, or compartment by compartment.
However, we can skip certain files that we don't want to rewrite.

### Skipping `*_tmpl.c` Template Files

In `dav1d` (with our chosen compartments),
we ideally want to rewrite everything in `src/` and `tools/`, but not in things like `tests/`.
Thus, we pass all of the files starting with `src/` and `tools/` to `ia2-rewriter`.
However, `ia2-rewriter` doesn't work on `dav1d`'s templated files,
files named `*_tmpl.c` that are compiled multiple times
with different `-D` defines for different bitdepths.
This doesn't work with `ia2-rewriter` because the DSP function pointer signatures
`dav1d` defines are different depending on the bitdepth,
so `ia2-rewriter` ends up generating conflicting definitions
for the `struct IA2_fnptr_*`s defined in the callgate header.
It's not clear how to get around this
(in `rav1d`, we redefined the function signatures to a common signature
and casted concrete function pointers to the common type),
so the simplest solution was to filter out all of the `*_tmpl.c` files,
all of the `*.h` files corresponding to the `*_tmpl.c` ones,
and a few other troublesome files (`msac.{h,c}`),
which also defined differing function pointer types.

With these skipped files, `ia2-rewriter` successfully runs,
though with many warnings, which we'll discuss a bit later.

### Running `ia2-rewriter`

To actually run `ia2-rewriter`, we invoke it like this:

```sh
ia2_rewriter \
    --output-prefix $ia2_cwd/callgate_wrapper \
    --root-directory $cwd \
    --output-directory $ia2_cwd \
    -p $cc_db_dir \
    --extra-arg -isystem \
    --extra-arg include-fixed \
    --extra-arg -isystem \
    --extra-arg $(llvm-config --libdir)/clang/18/include \
    $srcs[@]
```

Here, `$cwd` is the `dav1d` repo directory (so the root directory),
`$ia2_cwd` is the `dav1d-ia2` repo directory (so the output directory),
the output prefix for extra generated callgate files is `$ia2_cwd/callgate_wrapper`,
we pass the directory the `compile_commands.json` file is in with `-p`,
we pass the filtered list of source files,
and we pass a few extra args that `ia2-rewriter` seems to require
(otherwise it gets errors about not being able to find `stddef.h`).

### Reverting Intra-Compartment Rewrites with Errors

When we run this, we gets a lot of output about what's being rewritten,
including a bunch of warnings about function pointer types that must be rewritten manually.
These are DSP function pointers, the same bitdepth ones we tried to skip,
and they can't be rewritten here since they are defined in macros.
However, since none of these function pointers are cross-compartment,
they don't actually have to be rewritten.
`ia2-rewriter` doesn't do an escape analysis of exposed
and cross-compartment function pointers, so it doesn't realize this, but we know.
Thus, we can simply revert (as in `git checkout --`)
all of the changes to files that we don't need to rewrite
since they're not involved with the cross-compartment boundary.

Thus, we `git add` the `src/` files we want to keep:
* `data.c`
* `data.h`
* `lib.c`
* `log.c`
* `obu.c`
* `picture.c`
* `ref.c`
* `ref.h`

and then run `git checkout -- src/*` to revert
the changes in all of the other `src/*` files.

This fixes the vast majority of `ia2-rewriter` errors/warnings.

### Manual Changes

However, there are still a select few others that we have to then manually fix.

#### `IA2_IGNORE`

Some function pointers are rewritten,
but are actually passed to `libc` functions like `pthread_once` or `pthread_create`.
Since we can't rewrite those functions,
and because `libc` is in the shared compartment anyways,
we can avoid rewriting these function pointers and calling them through call gates.
We can tell `ia2-rewriter` to skip these by wrapping the function pointers in `IA2_IGNORE`.
This is done with `init_internal` (twice) and `dav1d_worker_task`.

Also note that we'll have to `#include <ia2.h>` in files that
use macros like `IA2_IGNORE`, which will work with the
`'-I' + join_paths(ia2_path, 'runtime/libia2/include')` we added earlier.

#### `IA2_SIGHANDLER`

A similar things occurs with signal handlers,
as they are function pointers passed to `sigaction`.
To fix this, we can wrap the function pointer in `IA2_SIGHANDLER`
and call `IA2_DEFINE_SIGHANDLER(signal_handler, PKEY)`
with the compartment/pkey that the signal handler should run in.
Importantly, `IA2_DEFINE_SIGHANDLER` must be called outside of any functions.
It defines a function itself, and nested functions aren't allowed in C,
so it must be in the global scope.  Otherwise, it will segfault.
 
#### Rewrites in Macros

There are some rewrites that happen in macros,
which `ia2-rewriter` can't reason about well enough to rewrite.
For `dav1d`, this is the `validate_input_or_ret` macro,
and we just have to manually add some `IA2_ADDR` wrappers
as `ia2-rewriter` would have done if it weren't inside a macro.

#### `va_list`

`ia2-rewriter` generates `struct __va_list_tag *`
as the trailing type in a vararg function pointer in the callgate header.

Note that vararg function pointers aren't fully supported,
but are supported with half call gates,
where the function pointer will run in the shared compartment.
Since the vararg function pointer `dav1d` uses is just for logging,
and because the `fprintf` logging is in `libc` and shared anyways, this is fine.

However, as the function pointer type is defined with `struct __va_list_tag *`,
and because `__va_list_tag` is a private type, this will fail to compile.
Instead, we can use `va_list` rather than `struct __va_list_tag *`.

Fixing this properly in `ia2-rewriter` (by emitting just `va_list`)
is tracked in [#429](https://github.com/immunant/IA2-Phase2/issues/429).

#### `dlsym`

`dav1d` calls `dlsym` at one point to conditionally call a function
if it exists at runtime (specifically `__pthread_get_minstack`).
We don't have access to this function,
so it can't berewritten and a call gate generated for it,
so we have the same situation as above with a half call gate.
But this is again a `libc` function, so it's going to be in
the shared compartment anyways, so this is not a problem.
This means we can just create a `struct IA2_fnptr_*`
from it without a double-sided call gate, i.e.,
we can find the mangled (but stable) `IA2_fnptr_*` name
for this function pointer type and then create an instance of it with:

```c
(struct IA2_fnptr__ZTSFmPK14pthread_attr_tE) { .ptr = dlsym(RTLD_DEFAULT, "__pthread_get_minstack") }
```

Fixing this properly in `ia2-rewriter`
(by using a dedicated macro instead of having to look up the mangled type suffix)
is tracked in [#425](https://github.com/immunant/IA2-Phase2/issues/425).

## Compiling

Now that we've rewritten `dav1d`, both automatically and manually,
we can start compiling the compartmentalized version.
In doing this, we need more build system changes, both in `meson` and outside of it,
as well as more source changes to fix runtime compartment violations.

### `meson` Changes

There are a host of other flags and new generated call gate sources
that must be built for a compartmentalized build,
but that we don't need for the earlier build
that was for generating a `compile_commands.json`.
We can separate these in `meson` with an option in `meson_options.txt`,
which we named `ia2_enable` (a `bool`).
We also have an `ia2_path` option, which is what also turns on
the ia2 args we added for the `compile_commands.json` earlier.

We can divide these extra args into a few categories.
First, there are args for each compile command
and there are args for each link command.
Then there are args that are for every command,
or only for building `libdav1d.so` (`src/`, `lib.c`, or compartment 2)
or `dav1d` (`tools/`, `dav1d.c`, or compartment 1).

#### Global Args

For global compile args, we add these:

* `-include`, `join_paths(dav1d_src_root, 'callgate_wrapper.h')`:
    Here, this `callgate_wrapper.h` is the header of the call gates.
    It's named this specifically because we set our `--output-prefix` to this.
* `-Werror=incompatible-pointer-types`:
  To detect errors around function pointers and their wrapped call gates types.
* `-DIA2_PERMISSIVE_MODE=1`:
  If we want permissive mode, or we can disable it.
  Note that this is a `#define` we added to `dav1d` specifically
  to make it easier to selectively `#include <permissive_mode.h>`,
  which is the normal IA2 way of turning on permissive mode.

For global link args, we add these:

* `-fPIC`
* `-pthread`
* `join_paths(ia2_path, 'build/runtime/libia2/liblibia2.a')`
* `-Wl,--wrap=pthread_create`
* `-Wl,-z,now`
* `-Wl,-z,relro`
* `'-Wl,-T' + join_paths(ia2_path, 'runtime/libia2/padding.ld')`
* `'-L' + join_paths(meson.project_build_root(), 'src/')`:
  This is the directory that `dav1d` builds its `*.so`s in,
  so we build any extra `*.so`s here, too,
  which simplifies some of the linking and loading.
* `-lcallgates`:
  This links `libcallgates.so`, which we build separately.
  This could be integrated into `meson`, but we didn't bother,
  and just built it in `rewriter.py`:

  ```sh
  cc \
    -shared \
    -fPIC \
    -Wl,-z,now \
    callgate_wrapper.c \
    -I $ia2_path/runtime/libia2/include/ \
    -o $rpath / "libcallgates.so \
  ```

  where `$rpath` is the above path where the `*.so`s are stored,
  that we `-L` link to and will also `-rpath` load from.

These args are added with `add_project_arguments` for the compile args,
and with `add_project_link_arguments` for the link args.
Note that `add_project_link_arguments` is not arguments for the linker,
it is arguments for the `cc` link command.

#### `libdav1d.so` Compartment 2 Args

For link args, we add these:

* `-Wl,@../callgate_wrapper_$pkey.ld`

Here, `$pkey` is the hardcoded (2) pkey/compartment for `libdav1d.so`.

And while we don't need any extra compile args for this compartment,
we do need to disable the `-Wl,--no-undefined` link arg that `meson` adds by default.
The way IA2 builds things, this compartment's DSO needs symbols
from the main compartment, specifically from the runtime (from `INIT_RUNTIME`).
Thus, we have to disable `-Wl,--no-undefined`.
There's no flag to turn it off (like a `-Wno-*` arg),
but we can do it in meson by setting `override_options: ['b_lundef=false']`
in the `library` call that links `libdav1d.so`.

#### `dav1d` CLI Compartment 1 Args

`dav1d` also doesn't need any extra compile args,
but it needs a lot of extra link args:

* `-Wl,@../callgate_wrapper_$pkey.ld`:
  Again, `$pkey` is the hardcoded (1) pkey/compartment for the `dav1d` CLI.
* `-Wl,--wrap=main`
* `'-Wl,--dynamic-list=' + join_paths(ia2_path, 'runtime/* libia2/dynsym.syms')`
* `-Wl,--export-dynamic`
* `-lpartition-alloc`:
  For `libpartition-alloc.so`, which we built as part of IA2,
  and which supports partitioned, per compartment allocations.
* `-Lsrc/`:
  Where we put `libpartition-alloc.so`.
* `-Wl,-wrap,calloc`
* `-Wl,-wrap,free`
* `-Wl,-wrap,malloc`
* `-Wl,-wrap,memalign`
* `-Wl,-wrap,posix_memalign`
* `-Wl,-wrap,pvalloc`
* `-Wl,-wrap,realloc`
* `-Wl,-wrap,valloc`
* `-Wl,-wrap,malloc_usable_size`
* `-Wl,-wrap,realpath`
* `-Wl,-wrap,strdup`
* `-Wl,-wrap,strndup`
* `-Wl,-wrap,getcwd`
* `-Wl,-wrap,asprintf`
* `-Wl,-wrap,vasprintf`:
  All of the above are the `libc` functions that we want
  `libpartition-alloc.so` to wrap and partition correctly.
* `/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2`:
  For some reason, there started to be a link error saying
  `__tls_get_addr@@GLIBC_2.3`, a TLS symbol from `glibc`, is no longer defined.
  Adding the loader `ld.so` as an explicit argument seemed to fix this.

  Fixing this properly is tracked in [#456](https://github.com/immunant/IA2-Phase2/issues/456).

### `libia2` Changes

#### `pthread_atfork` Undefined

At the same as `__tls_get_addr@@GLIBC_2.3` started being undefined,
so did `pthread_atfork` when compiling with permissive mode (`-DIA2_PERMISSIVE_MODE=1`).
`pthread_atfork` is called in `libia2` in `permissive_mode_init`
in order to add a permissive mode fork handler.
Simply commenting out this call fixes the linking issue,
though it's still unclear why this is happening.
When running `dav1d` as single-threaded (or without permissive mode),
this shouldn't be an issue, but it still needs a proper solution.

Fixing this is tracked in [#455](https://github.com/immunant/IA2-Phase2/issues/455).

### `-fvisibility=hidden`

`dav1d` compiles with `-fvisibility=hidden`,
making all symbols hidden by default and explicitly marking symbols 
to publically export with `__attribute__((visibility("default")))`.
However, this doesn't yet fully work with IA2.
Part of this (where IA2 runtime functions defined in `INIT_RUNTIME`
now are marked with default visibility) has already been fixed.
But the part where call gates for non-exported
address-taken functions are defined in the call gates DSO,
and thus either need to be default visibility
or defined in the same source file as the function they're wrapping
hasn't been fixed yet (tracked in [#443](https://github.com/immunant/IA2-Phase2/issues/443)).

Thus, for now we disable `-fvisibility=hidden` in the `meson.build`.
This has the risk of making all of these symbols public,
which means they could potentially clash and be overridden,
which is why just turning off `-fvisibility=hidden` is not a full solution.
But for just this demo and showing that compartmentalizing `dav1d` works, this suffices.

## Running

Now that we can finally compile a compartmentalized `dav1d` and `libdav1d.so`,
we can start running it, only to run into a ton of segfaults.

### "Local Exec" TLS Model Segfault

When accessing `ia2_stackptr_0`, a TLS variable,
there would be a segfault sometimes (on certain builds).
We determined that this was due to `libdav1d.so` and the `dav1d` CLI
using different TLS models for `ia2_stackptr_0`,
which calculate its address differently,
so when TLS variables were `mprotect`ed
and then the wrong address used, a segfault would occur.

A workaround for this is moving the `ia2_stackptr_0` assignment
to `libia2` rather than in `INIT_RUNTIME` and thus in the `dav1d` CLI compartment.
Putting it in a different library forces the "Local Exec" TLS model to not be used.
Thus, this fully fixes this issue,
but we should solve the "Local Exec" TLS model issue more properly,
as it could also cause other problems with other TLS variables.

Fixing this properly is tracked in [#457](https://github.com/immunant/IA2-Phase2/issues/457)
and the workaround is [#458](https://github.com/immunant/IA2-Phase2/pull/458).

### `pad-tls` Padding

Since TLS sections are `mprotect`ed separately,
they need to be padded so that they can be `mprotect`ed.
This applies to the `dav1d` CLI binary as well as all of its dependencies,
including `libdav1d.so`, but also other implicit dependencies.

These can be determined by `ldd`:

```shell
> ldd ../dav1d-ia2/build/tools/dav1d
        linux-vdso.so.1 (0x00007fff685eb000)
        libcallgates.so => /home/kkysen/work/rust/ia2/../dav1d-ia2/build/tools/../src/libcallgates.so (0x000072e5e553b000)
        libdav1d.so.7 => /home/kkysen/work/rust/ia2/../dav1d-ia2/build/tools/../src/libdav1d.so.7 (0x000072e5e5200000)
        libpartition-alloc.so => /home/kkysen/work/rust/ia2/../dav1d-ia2/build/tools/../src/libpartition-alloc.so (0x000072e5e505d000)
        /lib64/ld-linux-x86-64.so.2 (0x000072e5e55d2000)
        libm.so.6 => /home/kkysen/work/rust/ia2/../dav1d-ia2/build/tools/../src/libm.so.6 (0x000072e5e5454000)
        libc.so.6 => /home/kkysen/work/rust/ia2/../dav1d-ia2/build/tools/../src/libc.so.6 (0x000072e5e4e00000)
        libstdc++.so.6 => /lib/x86_64-linux-gnu/libstdc++.so.6 (0x000072e5e4a00000)
        libgcc_s.so.1 => /lib/x86_64-linux-gnu/libgcc_s.so.1 (0x000072e5e503d000)
```

Any of these DSOs need to have their TLS sections padded.
To do this, `rewrite.py` parses this `ldd` output,
runs `pad-tls --allow-no-tls` on `dav1d` and all of the `*.so`s `ldd` shows,
and then places these `pad-tls` padded libaries in `$rpath`
so that the loader `ld.so` sees them.
The `RPATH`/`RUNPATH` is already set by `dav1d`'s `meson` to be `$ORIGIN/../src`, so `dav1d-ia2/build/src/`,
so we reuse it for the padded libraries.

This obviously does not work with `linux-vdso.so.1`, as it has no path.
And it doesn't work with `ld.so`, as it's the loader
(There might be a way to do this, but I'm not sure how yet.
Also, see [#449](https://github.com/immunant/IA2-Phase2/issues/449) for if we need to do this.).
It does work with `libc.so`, `libm.so`, `libpartition-alloc.so`,
`libcallgates.so`, and `libdav1d.so`, but it still doesn't work for
`libgcc_s.so` and `libstdc++.so` yet, for which I'm not sure why.

### Shared Stack Variables

IA2 doesn't provide a way to share stack variables between compartments,
because `mprotect` only works at page granularity,
and this doesn't work for variables on the stack.
This can be fixed by using `shared_malloc` from `partition-alloc` instead,
but since all of these stack variables in `dav1d` were declared in `main`,
we just made them globals instead, where we can use
`IA2_SHARED_DATA` to move them into their own shared section.
