# Source Rewriter Design

In Phase I, we used a custom compiler to insert call gates at compartment
transitions. In Phase II, we treat dynamic shared objects (DSOs) as compartments
and use standard linking techniques to insert call gates between
intercompartment direct calls. For indirect calls, we rely on user annotations
for address-taken functions. Instead of requiring that function pointer
annotations be added manually, we instead plan on creating a tool that
rewrites source files to make the necessary changes.

This document goes over what the source rewriter does and what build artifacts
it generates. For more detailed on using the source rewriter, see [`usage.md`](usage.md).

## Rationale

Our initial approach relied on analyzing and rewriting header files to (1)
create an output `.c` for the call gate wrappers and (2) assist the user in
adding the function pointer annotations (by rewriting function pointer types to
trigger compiler errors where changes are needed). This approach has two issues.

First, a header's AST can change depending on the preprocessor context when it's
included. We also use the same set of `-D`s for all `.h`s, but the correct
solution is to use the exact set of flags used to compile the `.c` that
includes each `.h`.

The second problem is that code that makes heavy use of function pointers is
burdensome to annotate manually. Even in the case where only a small set of the
function pointers in a code base are used, the rewritten `.h`s trigger compiler
errors at every site without annotations. Also, some of the annotations need the
pointer's function signature specified with the mangled type name, which is a
non-starter.

## Goals

By analyzing and rewriting both `.c`s and `.h`s, we can avoid these issues. The
goal here will be to (1) create an output `.c` for call gate wrappers and (2)
insert function pointer annotations in the input source files. While we'll still
rewrite function pointer types as opaque structs, by rewriting everything at
once we can simplify the user-facing annotations. We'll initially focus on the
case with one rewritten `.c` for each input `.c` and attempt to insert the
annotations such that the rewritten `.c` can be compiled with the same flags.

## Source rewriting process

The goals above can be broken down into these subgoals.

1. Define call gates for direct compartment transitions
2. Add call gates for address-taken functions
3. Add call gates for indirect callsites

The rewriter will take all source files as input along with which compartment
each belongs to. It will generate one output `.c` and one `.h` for use with all
compartments. It will also generate compartment-specific linker args and
`objcopy --redefine-syms` files.

### Define call gates for direct transitions

The rewriter will work in two phases. In the first phase, it'll parse all source
files and keep track of all functions declared or defined and their
compartment. Calls to functions not defined in any of the input sources (e.g.
libc, prebuilt libraries) will be assumed to remain in the caller's compartment.

In phase two, we'll generate the call gates for all potential direct transitions
by taking the set of functions declared in a compartment and removing those that
are defined in that same compartment. Since we know all the caller and target
compartment we can hard-code the pkey in each call gate wrapper. These wrappers
are inserted with `ld --wrap`, so their names must start with `__wrap_`.

Since call gates have hard-coded caller pkeys, we'll need some extra steps to
handle the case where a function is called from multiple compartments. In this
case we'll have multiple call gates for this function so we'll need to
distinguish their names since they must be global ELF symbols. We can do this by
adding a suffix to the generated call gate names (e.g
`__wrap_$FN_NAME_from_$CALLER_PKEY`) and redefining the original function symbol
in the object files by using `objcopy` before linking.

For example, if compartments 1 and 2 both call `foo` in f3, we'll need

```sh
gcc -c comp1.c -o comp1.o
gcc -c comp2.c -o comp2.o

# The implementation will use --redefine-syms=filename instead of redefining
# symbols one by one
objcopy --redefine-sym foo=foo_from_1 comp1.o
objcopy --redefine-sym foo=foo_from_2 comp2.o

gcc -fPIC -shared comp1.o -o libcomp1.so -Wl,--wrap=foo_from_1
gcc -fPIC -shared comp2.o -o libcomp2.so -Wl,--wrap=foo_from_2
```

and we'll generate the following call gate wrappers:

```c
asm(__wrap_foo_from_1:
      wrpkru pkey3
      call foo
      wrpkru pkey1
      ret);

asm(__wrap_foo_from_2:
      wrpkru pkey3
      call foo
      wrpkru pkey2
      ret);
```

### Call gates for address-taken functions

Call gates for address-taken functions come in two variants: wrappers for
internal and external linkage. Again, the tool has two phases. The first phase
will search for functions that have their address taken, making sure to store
functions in different sets based on their linkage. For internal linkage, we need
to track the files where the function had its address taken. For external
linkage, we just need the function name. It'll also rewrite these function
pointer expressions as `IA2_FN(fn_name)`.

In the second phase, we'll generate the call gate wrappers. Since a function
pointer may be freely moved between compartments, the wrapper will always expect
the default (i.e. untrusted) pkey 0 (the wrappers on indirect callsites will
change to this pkey before calling any pointer). A function is defined in a
fixed compartment so the target pkeys in wrappers for pointers will also be
fixed. Since the caller/target pkeys cannot take different values, we can just
name the wrappers for the external linkage case `__ia2_$FUNCTION_NAME` and write
them to the output `.c`. The `IA2_FN` macro mentioned above will then take care
of prepending `__ia2_` and the output header will contain declarations for the
wrappers.

For internal linkage (i.e. static), for functions that have their address taken, the
same caller/target pkey logic applies so we can also name their wrappers by
prepending `__ia2_`. However, since the wrappers call `static` functions, they
must be defined in the same translation unit as the `static` function. To avoid
cluttering up the input source files, we'll define the following macro in the
output header for each of these functions and invoke them in the corresponding
input sources.

```c
#define IA2_DEFINE_WRAPPER_FN_NAME \
    asm(__ia2_fn_name:                    \
          wrpkru pkey                     \
          call fn_name                    \
          wrpkru 0                        \
          ret);
```

### Call gates for indirect callsites

We also need call gates on indirect callsites. Each compartment might not
trust pointers coming from other compartments, so these callsites need to change
the pkey from the caller's pkey to untrusted pkey 0. Call gate wrappers for
appropriate functions will then change the pkey from untrusted to their callee's
pkey.

As the rewriter finds function pointer types, it'll assign each type a unique
integer identifier `N`. Since callsites in different compartments have different
caller pkeys, wrappers need to be specific to both the function pointer type
(i.e. pointee function signature) and the caller compartment.

In the rewriter's first phase, we'll rewrite indirect callsites as
`IA2_CALL(ptr_expr, N)(args)`, where `N` is an integer identifying the signature
of the function pointer.

In the second phase, we'll generate the wrappers (named
`__ia2_indirect_callgate_$N_pkey_$CALLER_PKEY`) in the output `.c`. Note there
will be up to `NUM_FN_SIGNATURES * NUM_COMPARTMENTS` different macros, but there may
be less depending on the callsites actually found in the input sources. Again,
the output header will declare these wrappers.

```c
// In the output .h
// The wrappers are declared as `extern char` for simplicity
extern char __ia2_indirect_callgate_0_pkey_1;
extern char __ia2_indirect_callgate_1_pkey_1;
// The IA2_CALL macro uses these to cast the `extern char` to the correct
// function-pointer type
#define IA2_TYPE_0 unsigned int (*)(unsigned int, unsigned int)
#define IA2_TYPE_1 unsigned int (*)(unsigned int)


// In the output .c
asm(.global __ia2_indirect_callgate_0_pkey_1 ...);
asm(.global __ia2_indirect_callgate_1_pkey_1 ...);

// In the input .c

    // The second macro parameter is the function-pointer type id and the pkey
    // is obtained from the -DPKEY= used when compiling this file
    int x = IA2_CALL(fn_ptr, 0)(arg0, arg1);
    IA2_CALL(fn_ptr2, 1)(arg2);
```
