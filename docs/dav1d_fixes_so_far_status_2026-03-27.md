# Dav1d Fixes So Far: Commit Status and What Each Fix Did

Date: 2026-03-27
Branch inspected: `fix/shared-tls-tcb-rationale-docs`
HEAD: `da780290f6a2c2f122263a0650f30f2580babaa7`

## Short Answer

Your assumption is only partly correct.

- The baseline TLS/TCB fixes that advanced us from `main` to this branch are **already committed**.
- Additional runtime carveout experiments (DTV, loader-heap, libc tuning symbols, etc.) are **currently uncommitted** in the working tree.

## What Is Already Documented

There is already substantial writeup in these docs:

- `docs/git-diff-main-da780-e664-tutorial.md`
- `docs/dav1d_fault_addresses_first_principles.md`
- `docs/conversation_repro_log_2026-03-27.md`
- `docs/dav1d_compartment_mismatch_writeup.md`

This file is specifically a **status/inventory** of which fixes are committed vs uncommitted.

## Committed Fix Chain (Main -> Current Branch HEAD)

`git log main..HEAD` shows 4 commits:

1. `a6e769880` - `libia2: keep TCB TLS page shared across compartments`
- Introduced x86_64 handling to keep thread-pointer/TCB ABI-critical TLS page shared (`pkey 0`) instead of compartment-private.
- Added startup and thread-path hooks to re-assert that sharing.
- Goal: avoid `%fs` ABI reads (for example stack canary path) faulting under cross-compartment PKRU states.

2. `4cc8315dd` - `libia2: avoid over-broad TLS shared-page carveout`
- Tightened carveout behavior so the shared-page policy does not unintentionally widen beyond intended overlap.
- Goal: keep fix targeted and preserve isolation budget.

3. `633acec3c` - `libia2: keep pre-refactor TLS protection path on aarch64`
- Preserved original aarch64 behavior while x86_64 TLS policy evolved.
- Goal: avoid x86_64-specific refactor side-effects on aarch64.

4. `da780290f` - `libia2: avoid redundant thread TCB pkey_mprotect`
- Removed redundant per-thread TCB retag path that could conflict with tracer monotonicity / repeated re-tag expectations.
- Goal: keep shared-page policy correct without repeated unnecessary pkey changes.

## Current Uncommitted Runtime Changes (Working Tree)

These are present as unstaged modifications and are not yet part of HEAD:

- `runtime/libia2/ia2.c`
- `runtime/libia2/include/ia2_internal.h`
- `runtime/libia2/init.c`
- `tests/CMakeLists.txt`
- submodule dirty state: `external/glibc`

High-level content of the uncommitted runtime diffs:

- Added `ia2_unprotect_thread_dtv_page()` and `ia2_unprotect_loader_heap_maps()` hooks (x86_64).
- Called those hooks in `ia2_start()` after compartment setup.
- Added targeted shared-range helpers and symbol-based carveout plumbing in `protect_pages()`.
- Added x86_64 carveout logic for:
  - ld.so TLS generation metadata page,
  - selected libc memmove/memset tuning globals.
- Increased shared-range capacity constant to accommodate extra carveouts.
- Added test registration for `tests/tls_one_page_repro` in `tests/CMakeLists.txt`.

These uncommitted changes correspond to later-stage investigation after the committed TCB/TLS fixes, and are aimed at getting past subsequent decode blockers.

## Why This Distinction Matters

- **Committed chain (`main..da780`)**: foundational TLS/TCB policy fixes.
- **Uncommitted changes**: follow-on attempts to handle additional runtime metadata and then decode-path heap ownership mismatch.

So if you reset to clean HEAD on this branch, you keep the baseline TLS/TCB fixes, but you lose the newer DTV/loader-heap/libc-symbol carveout experiments unless you commit/stash them.

## Reproduce the Status Check

Commands used for this inventory:

```sh
git branch --show-current
git show -s --format='%H %ci %s' HEAD
git log --oneline --decorate main..HEAD
git status --short
git diff --stat
```
