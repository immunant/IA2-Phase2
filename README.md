# IA2 Sandboxing Runtime

IA2 (or Intent-capturing Annotations for Isolation and Assurance) is a runtime and set of tools for compartmentalizing C/C++ applications to provide coarse spatial memory-safety.

Applications typically use many third-party C/C++ libraries that may introduce memory-safety vulnerabilities if developers don't have the resources to exhaustively audit them. Putting everything in a single process means that a vulnerability in one library can compromise another and that's a problem for programs that handle security-sensitive information. The IA2 sandbox splits applications into isolated compartments and uses CPU hardware features (Memory Protection Keys on x86-64) to forbid cross-compartment memory accesses. Compartments are delineated along pre-existing boundaries (at the shared library level) avoiding the need to rearchitect a codebase and source-code annotations are used to mark variables that are intentionally shared between compartments. The runtime also ensures that each set of shared libraries in a compartment uses a distinct region of memory for its stack, heap, static and thread-local variables.

## Sandboxing workflow

IA2 uses source-code transformations as part of the build process to sandbox programs. Our [rewriter tool](docs/source_rewriter.md) processes a codebase's source files before each build to produce a set of intermediate sources with annotations for compartment transitions at cross-DSO calls. These intermediate sources are then passed on to a build system using off-the-shelf compilers with some additional flags and the IA2 runtime is linked in to create the compartmentalized program. See the [design doc](docs/design.md) for details.

This workflow treats intermediate sources as build artifacts so the only annotations that need to be checked-in are those that can't be inferred. These are primarily annotations for shared variables and cross-library indirect calls when round-tripping function pointers through `void *`. For the latter the rewriter also does type-system transformations to turn missing annotations into compiler errors and avoid accidental miscompartmentalization. In compartmentalized programs cross-compartment memory accesses kill the process, but they also support a permissive mode which just logs the accesses to aid developers adding shared variable annotations.

## Features

- **Hardware checks** Coarse spatial memory safety enforced using CPU hardware features to reduce the runtime cost of bounds checking. On x86-64 this means Memory Protection Keys ([`wrpkru`](https://www.kernel.org/doc/html/next/core-api/protection-keys.html)) for memory permissions and Control-flow Enforcement Technology (`endbr` and [SHSTK](https://docs.kernel.org/next/x86/shstk.html)) to prevent call gate misuse. Support for Aarch64 using Memory Tagging Extensions is in-progress.
- **Minimal developer-facing annotations** Compartment transition annotations that can be inferred are automatically added by the rewriter and only appear in build artifacts. This reduces the developer-facing annotations while allowing developers to audit source-code changes made by the rewriter if necessary.
- **Toolchain-independent** Compartmentalized programs are built with off-the shelf compilers and linkers.

## Usage

See [this doc](docs/build_instructions.md) for instructions on building the tools and tests in this repo. For more detailed instructions on the compartmentalization process see the [usage doc](docs/usage.md).
