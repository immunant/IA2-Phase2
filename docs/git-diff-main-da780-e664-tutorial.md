# What `main..ad0606` and `ad0606..e664` Actually Change (TLS/TCB Deep Dive)

This is not a Git syntax tutorial. It is a code and runtime behavior tutorial for the two diffs you asked about.

## Scope

This writeup explains:

- what runtime problem each code change is addressing,
- what the key TLS/TCB terms mean,
- why each change plausibly fixes the observed fault progression,
- what tradeoffs each change introduces.

Primary code paths touched:

- `runtime/libia2/ia2.c`
- `runtime/libia2/init.c`
- `runtime/libia2/threads.c`
- `runtime/libia2/include/ia2_internal.h`

## TL;DR

`main..ad0606` introduces the first x86_64 TLS carve-out policy that keeps the TCB page shared (pkey 0), avoids over-broad carveouts, preserves aarch64 behavior, and removes redundant per-thread retagging.

`ad0606..e664` extends that policy to include the TCB *neighborhood* and static-TLS prefix behavior needed for dav1d single-thread decode, adds a mapping-level retag pass using `/proc/self/maps`, and hardens thread creation timing around PKRU state.

## Terms You Need (in plain language)

`PT_TLS`
- ELF program header describing a module's TLS template and size.
- Reference: [ELF gABI PT_TLS](https://refspecs.linuxfoundation.org/elf/gabi4+/ch5.pheader.html).

`TCB` (Thread Control Block)
- Thread runtime state pointed to by thread pointer on x86_64 (`%fs` base).
- In glibc x86_64, initial TCB size is tied to `struct pthread` (`TLS_INIT_TCB_SIZE`).
- Reference: [glibc tls.h](https://codebrowser.dev/glibc/glibc/sysdeps/x86_64/nptl/tls.h.html#L104).

`%fs` stack canary access
- Stack protector guard loads are `%fs`-relative (`STACK_CHK_GUARD`).
- Reference: [glibc stackguard-macros.h](https://codebrowser.dev/glibc/glibc/sysdeps/x86_64/stackguard-macros.h.html#L3).

`PKRU` / pkeys
- Hardware permission state and per-page protection key tagging.
- IA2 uses `pkey_mprotect` policy to retag pages by compartment.
- Reference: [pkey_mprotect(2)](https://man7.org/linux/man-pages/man2/pkey_mprotect.2.html), [pkeys(7)](https://man7.org/linux/man-pages/man7/pkeys.7.html).

`__tls_get_addr`
- DTV (Dynamic Thread Vector) is a per-thread table that maps TLS module IDs to this thread's concrete TLS block pointers.
- TLS resolution is the runtime step that turns a TLS symbol reference into an address for the *current thread*.
- `__tls_get_addr` is the glibc runtime path used for this resolution; it consults DTV plus loader-maintained TLS metadata.
- Reference: [glibc dl-tls.c __tls_get_addr](https://codebrowser.dev/glibc/glibc/elf/dl-tls.c.html#L1007), [glibc `THREAD_DTV` macro in tls.h](https://codebrowser.dev/glibc/glibc/sysdeps/x86_64/nptl/tls.h.html#L166).

`ia2_stackptr_N`
- IA2 TLS stack-pointer slots used by callgate transitions.
- If these pages are compartment-private at the wrong time, cross-compartment stack switching breaks.
- Context: [IA2 threading doc](./threading.md).

## Ramp-Up: Why the 1-2 Page TLS Intuition Breaks

This section is self-contained and explains why "just share 1 page" (or "1-2 pages") near thread pointer feels reasonable, but is not a safe invariant.

### Step 1: Why the assumption feels plausible

If you are debugging an x86_64 `%fs` fault, it is natural to think:

- `%fs:0x28` canary access is in the TCB area.
- TCB sounds like a compact structure.
- Therefore, keeping the TCB page shared should be enough.

That is a good first approximation for triage.

### Step 2: What that assumption is *actually* claiming

The assumption is not about `PT_TLS` segment size in ELF files.

It is claiming a runtime layout invariant:

Examples of ABI-sensitive thread state include:

- `%fs` stack canary and pointer guard reads (`stack_guard`, `pointer_guard`).
- thread-local bookkeeping reads such as `THREAD_DTV()` used in TLS resolution.
- thread descriptor self-pointer and related `struct pthread` header accesses.

So the implicit claim is:

- all ABI-sensitive `%fs`-reachable thread state IA2 must allow across compartment switches fits inside a fixed tiny neighborhood (1-2 pages) around TP/TCB.

TP vs TCB clarification:

- TP (thread pointer) is the architectural base used by `%fs`-relative accesses.
- TCB is the runtime control structure anchored at/near TP.
- On glibc x86_64 (`TLS_TCB_AT_TP=1` in [tls.h](https://codebrowser.dev/glibc/glibc/sysdeps/x86_64/nptl/tls.h.html#L114)), TP points at the TCB location, so people often bundle them in shorthand as "TP/TCB".

That stronger claim is where things break.

### Step 3: Why the stronger claim does not hold

`PT_TLS` is only module TLS metadata/template. It does not guarantee final per-thread runtime placement boundaries relative to TP.

In this document, "TCB neighborhood" means the contiguous per-thread pages around TP that runtime/ABI TLS paths may touch, not just the single page containing `__builtin_thread_pointer()`.

Conceptual x86_64 layout (not to scale):

```text
lower addresses
    ...
    [ static TLS blocks for loaded modules ]   <- often accessed via negative TPOFF from %fs
    [ padding / alignment ]
TP-> [ tcbhead_t fields: dtv, stack_guard, pointer_guard, ... ]
    [ rest of struct pthread / thread descriptor ]
    [ implementation-defined adjacent memory ]
    ...
higher addresses
```

glibc also does not promise "TCB-neighborhood is one page":

- x86_64 TLS setup defines TCB in terms of implementation internals like `struct pthread`, not a fixed one-page ABI contract.
- glibc comments explicitly state that memory following TP can be unspecified.
- It is **not** correct to equate "TCB-neighborhood" with `sizeof(struct pthread)`: `sizeof(struct pthread)` only sizes the descriptor object, while the neighborhood relevant to TLS faults can also include static TLS pages below TP and alignment-dependent adjacent pages.
- runtime TLS resolution (`__tls_get_addr`) uses thread TLS bookkeeping (DTV and loader-related metadata), which can involve adjacent pages.

So a strict 1-page carveout can pass one workload and still fail another.

### Step 4: Realistic failure modes (without contrived tricks)

The assumption can fail even if no single library has huge `__thread` variables:

- Different sets of loader-linked DSOs change static TLS placement.
- Alignment/padding constraints push TLS objects across page boundaries.
- glibc implementation/version details alter `struct pthread` and neighboring layout.
- Thread creation timing under compartment PKRU can expose accesses before/after IA2 retag phases.

This is why `dav1d --version` can work while real decode still fails: decode executes deeper code paths that touch more TLS/runtime state.

### Step 5: How that maps to IA2 changes

The code evolution reflects this learning:

- `main..ad0606`: carve out TCB page + key callgate TLS page(s), tighten carveout correctness.
- `ad0606..e664`: stop assuming one page is enough; share TCB neighborhood/static-TLS prefix and re-apply neighborhood policy during init/thread setup.

In short: move from a single-page "ABI guess" to a broader runtime-layout-aware policy.

### Step 6: Tradeoff framing

Broader carveouts are a security tradeoff:

- Pro: avoids false compartment faults on ABI/runtime TLS accesses.
- Con: increases pkey0 shared surface.

That is why this is described as reliability-first and not maximal isolation.

### Resource Pack

Core specs and libc sources:

- [ELF gABI program headers (`PT_TLS`)](https://refspecs.linuxfoundation.org/elf/gabi4+/ch5.pheader.html)
- [glibc x86_64 TLS definitions (`TLS_INIT_TCB_SIZE`, TP notes)](https://codebrowser.dev/glibc/glibc/sysdeps/x86_64/nptl/tls.h.html)
- [glibc `struct pthread` layout](https://codebrowser.dev/glibc/glibc/nptl/descr.h.html)
- [glibc TLS resolution path (`__tls_get_addr`)](https://codebrowser.dev/glibc/glibc/elf/dl-tls.c.html#L1007)
- [glibc stack guard `%fs` access](https://codebrowser.dev/glibc/glibc/sysdeps/x86_64/stackguard-macros.h.html#L3)

Linux memory/protection references:

- [`pkey_mprotect(2)`](https://man7.org/linux/man-pages/man2/pkey_mprotect.2.html)
- [`pkeys(7)`](https://man7.org/linux/man-pages/man7/pkeys.7.html)
- [`/proc/pid/maps` format (`proc_pid_maps(5)`)](https://man7.org/linux/man-pages/man5/proc_pid_maps.5.html)

Background deep dives:

- [Ulrich Drepper, *ELF Handling For Thread-Local Storage*](https://www.akkadia.org/drepper/tls.pdf)
- [MaskRay TLS overview](https://maskray.me/blog/2021-02-14-all-about-thread-local-storage)

## Diff 1: `main..ad0606` (4 commits)

Commit set on this side:

- [a6e769880](https://github.com/immunant/IA2-Phase2/commit/a6e769880720855a1abb7d63524760745ccf6286)
- [4cc8315dd](https://github.com/immunant/IA2-Phase2/commit/4cc8315dd7bf05f69cbfa69bd120a8df89fc1741)
- [633acec3c](https://github.com/immunant/IA2-Phase2/commit/633acec3c128ebb017707e69a7d1353c4ffc9aac)
- [ad0606ff0](https://github.com/immunant/IA2-Phase2/commit/ad0606ff01de5c01349014dc6dee307e510403b4)

### 1) `a6e769880`: keep TCB page shared

Problem being addressed:

- TLS retagging made `%fs`-reachable ABI state compartment-private.
- Compiler-generated stack canary loads can happen in transition windows.
- Result was early faults before intended callgate/runtime logic.

Key changes:

- Adds `ia2_unprotect_thread_pointer_page()` to retag TP/TCB page to pkey 0.
- Introduces TLS carve-out logic in `protect_tls_pages()` for:
  - `ia2_stackptr_0` page,
  - TCB page.
- Calls the unprotect helper in startup and thread setup paths.

Code references:

- [ia2_unprotect_thread_pointer_page (ad0606 blob)](https://github.com/immunant/IA2-Phase2/blob/ad0606ff01de5c01349014dc6dee307e510403b4/runtime/libia2/ia2.c#L114)
- [initial carveout policy in protect_tls_pages (ad0606 blob)](https://github.com/immunant/IA2-Phase2/blob/ad0606ff01de5c01349014dc6dee307e510403b4/runtime/libia2/ia2.c#L450)
- [startup call site](https://github.com/immunant/IA2-Phase2/blob/ad0606ff01de5c01349014dc6dee307e510403b4/runtime/libia2/init.c#L327)
- [thread setup path](https://github.com/immunant/IA2-Phase2/blob/ad0606ff01de5c01349014dc6dee307e510403b4/runtime/libia2/threads.c#L38)

Why this helps:

- It prevents PKRU isolation from blocking `%fs` ABI accesses that are not conceptually compartment-private.

Tradeoff:

- One more TLS page is shared at pkey 0, reducing compartment isolation for that page.

### 2) `4cc8315dd`: narrow an over-broad carveout

Problem being addressed:

- Carving by rounded page boundary could incorrectly classify a page as "contains ia2_stackptr_0" even when symbol was not in that module's true TLS range.

Key changes:

- Checks `untrusted_stackptr_addr` against `[start, end)` (actual TLS symbol span) rather than only page-rounded boundaries.
- Adds assert that split behavior is only for expected compartment.

Code reference:

- [range guard logic (ad0606 blob)](https://github.com/immunant/IA2-Phase2/blob/ad0606ff01de5c01349014dc6dee307e510403b4/runtime/libia2/ia2.c#L466)

Why this helps:

- Reduces accidental sharing / mis-tagging due to page alignment artifacts.

Tradeoff:

- Slightly stricter assumptions about symbol placement; but this is safer than broad carveout.

### 3) `633acec3c`: preserve aarch64 behavior

Problem being addressed:

- x86_64-oriented refactor risked behavior drift on aarch64.

Key changes:

- Splits logic by architecture and keeps pre-refactor aarch64 path intact.

Code reference:

- [aarch64 branch in `protect_tls_pages` (ad0606 blob)](https://github.com/immunant/IA2-Phase2/blob/ad0606ff01de5c01349014dc6dee307e510403b4/runtime/libia2/ia2.c#L401)

Why this helps:

- Reduces cross-arch regression risk while iterating x86_64 TLS policy.

Tradeoff:

- More conditional complexity in one function.

### 4) `ad0606ff0`: avoid redundant per-thread retagging

Problem being addressed:

- Re-applying shared-page retag in per-thread setup caused redundant `pkey_mprotect` behavior that conflicted with tracer monotonicity expectations.

Key changes:

- Adds `can_retag_shared_tls_pages = (ia2_get_compartment() == 0)` guard.
- Removes redundant thread-path explicit unprotect call and relies on TLS setup path.

Code references:

- [retag guard in `protect_tls_pages` (ad0606 blob)](https://github.com/immunant/IA2-Phase2/blob/ad0606ff01de5c01349014dc6dee307e510403b4/runtime/libia2/ia2.c#L504)
- [thread-path comment and removal](https://github.com/immunant/IA2-Phase2/blob/ad0606ff01de5c01349014dc6dee307e510403b4/runtime/libia2/threads.c#L39)

Why this helps:

- Prevents policy noise/regression from repeated retagging while keeping initialization semantics.

Tradeoff:

- Relies on invariant that post-init mappings are already in expected shared state unless explicitly corrected later.

## Diff 2: `ad0606..e664` (single commit)

Commit:

- [e6641c15e](https://github.com/immunant/IA2-Phase2/commit/e6641c15eaab4017c78e44bc305ce56be1bdf2d4)

High-level problem:

- Single TCB-page carveout was not enough for dav1d single-thread decode.
- TLS resolution/access paths (including `__tls_get_addr`) still touched TLS/loader-adjacent pages that remained compartment-private.

### A) New `ia2_unprotect_thread_pointer_mapping()`

What it does:

- Reads `/proc/self/maps`, finds writable mappings overlapping a window around thread pointer (`tcb_page - 8 pages` through `tcb_page + 1 page`), retags overlaps to pkey 0.

Code reference:

- [mapping helper (e664 blob)](https://github.com/immunant/IA2-Phase2/blob/e6641c15eaab4017c78e44bc305ce56be1bdf2d4/runtime/libia2/ia2.c#L125)

External refs:

- [/proc/pid/maps format](https://man7.org/linux/man-pages/man5/proc_pid_maps.5.html)

Why it helps:

- Catches adjacent TLS neighborhood pages not covered by a one-page TCB carveout.

Tradeoff:

- Linux `/proc` dependence.
- Runtime parsing overhead (small but non-zero).
- Window size (`8` pages below TCB) is policy-driven, not a formal ABI guarantee.

### B) `protect_tls_pages()` policy broadened from 2 pages to set-based carveout

What changed:

- `MAX_SHARED_TLS_PAGES` increases from `2` to `64`.
- New helper `add_shared_tls_page()` deduplicates pages.
- Carves out pages containing any active `ia2_stackptr_N` slot (not just `_0`).
- If TLS range overlaps TCB page, marks full static-TLS prefix up to TCB as shared.
- Sorts shared page list and applies compartment pkey to gaps.

Code references:

- [add_shared_tls_page helper](https://github.com/immunant/IA2-Phase2/blob/e6641c15eaab4017c78e44bc305ce56be1bdf2d4/runtime/libia2/ia2.c#L412)
- [new carveout policy block](https://github.com/immunant/IA2-Phase2/blob/e6641c15eaab4017c78e44bc305ce56be1bdf2d4/runtime/libia2/ia2.c#L533)
- [stackptr_N scan](https://github.com/immunant/IA2-Phase2/blob/e6641c15eaab4017c78e44bc305ce56be1bdf2d4/runtime/libia2/ia2.c#L548)
- [static TLS prefix carveout](https://github.com/immunant/IA2-Phase2/blob/e6641c15eaab4017c78e44bc305ce56be1bdf2d4/runtime/libia2/ia2.c#L565)

Why it helps:

- Addresses realistic multi-page TLS neighborhoods touched by loader/TLS resolution behavior.

Tradeoff:

- More shared pages means weaker isolation than strict per-compartment PT_TLS tagging.
- Bound (`64`) is still fixed policy, not dynamically unbounded.

### C) Init and per-thread call sites now also unprotect mapping neighborhood

What changed:

- `ia2_start()` calls both `ia2_unprotect_thread_pointer_page()` and `_mapping()`.
- `ia2_thread_begin()` calls `_mapping()` after per-thread TLS setup.

Code references:

- [init call sites (e664 blob)](https://github.com/immunant/IA2-Phase2/blob/e6641c15eaab4017c78e44bc305ce56be1bdf2d4/runtime/libia2/init.c#L327)
- [thread call site (e664 blob)](https://github.com/immunant/IA2-Phase2/blob/e6641c15eaab4017c78e44bc305ce56be1bdf2d4/runtime/libia2/threads.c#L40)

Why it helps:

- Ensures per-thread TLS mappings don't regress into private tagging around `%fs` neighborhood.

Tradeoff:

- Extra retag pass on thread startup.

### D) `pthread_create` wrapper temporarily switches to `PKRU(0)`

What changed:

- Around `__real_pthread_create`, code saves current PKRU, switches to `PKRU(0)`, calls real pthread create, restores previous PKRU.

Code reference:

- [pthread_create wrapper changes (e664 blob)](https://github.com/immunant/IA2-Phase2/blob/e6641c15eaab4017c78e44bc305ce56be1bdf2d4/runtime/libia2/threads.c#L112)

Why it helps:

- Avoids creating thread runtime state while restricted by a compartment PKRU that may block needed TLS/runtime accesses.

Tradeoff:

- Short critical window with broader permissions in creator thread.
- Correctness depends on strict restore discipline (which this code does explicitly).

### E) `runtime/partition-alloc/CMakeLists.txt`

Observed diff here is a formatting-only newline insertion, no functional compile flag change in this commit range.

Reference:

- [CMakeLists line region (e664 blob)](https://github.com/immunant/IA2-Phase2/blob/e6641c15eaab4017c78e44bc305ce56be1bdf2d4/runtime/partition-alloc/CMakeLists.txt#L116)

## Why this corresponds to the observed failure progression

The progression you observed is coherent with the code evolution:

- Step 1 (`main..ad0606`): fix obvious `%fs`/TCB single-page ABI faults and tighten carveout correctness.
- Step 2 (`ad0606..e664`): account for adjacent static TLS / loader neighborhood accesses that still faulted in decode path.

That is consistent with glibc details that matter here:

- Thread pointer points into/near `struct pthread` and adjacent implementation-defined memory.
- glibc explicitly says memory following TP is unspecified and TCB size is tied to `struct pthread` implementation internals.
- `__tls_get_addr` paths touch per-thread TLS bookkeeping (`THREAD_DTV`) and may require access beyond one page assumptions.

References:

- [glibc tls.h (`TLS_INIT_TCB_SIZE`, TP comments)](https://codebrowser.dev/glibc/glibc/sysdeps/x86_64/nptl/tls.h.html#L104)
- [glibc descr.h (`struct pthread`)](https://codebrowser.dev/glibc/glibc/nptl/descr.h.html#L130)
- [glibc dl-tls.c (`__tls_get_addr`)](https://codebrowser.dev/glibc/glibc/elf/dl-tls.c.html#L1007)
- [glibc stackguard `%fs` load](https://codebrowser.dev/glibc/glibc/sysdeps/x86_64/stackguard-macros.h.html#L3)

## Security and Policy Tradeoff Summary

Conservative side:

- Maintains compartment tagging for non-carveout TLS ranges.
- Keeps aarch64 path unchanged in this line.
- Deduplicates and orders carveouts to avoid accidental overlap behavior.

Less conservative side:

- Expands shared pkey0 surface in TLS neighborhood.
- Uses fixed-window heuristic around TP in mapping scan.
- Temporarily drops to `PKRU(0)` during `pthread_create` wrapper window.

Practical interpretation:

- This is a targeted reliability-first patch for single-thread decode stability under IA2 compartmenting.
- It intentionally spends some isolation budget to satisfy ABI/runtime TLS requirements that are not naturally compartment-local.

## Useful local inspection commands

```bash
git diff --stat main ad0606ff01de5c01349014dc6dee307e510403b4
git diff --stat ad0606ff01de5c01349014dc6dee307e510403b4 e6641c15eaab4017c78e44bc305ce56be1bdf2d4

git show a6e769880720855a1abb7d63524760745ccf6286 -- runtime/libia2/ia2.c runtime/libia2/init.c runtime/libia2/threads.c
git show ad0606ff01de5c01349014dc6dee307e510403b4 -- runtime/libia2/ia2.c runtime/libia2/threads.c
git show e6641c15eaab4017c78e44bc305ce56be1bdf2d4 -- runtime/libia2/ia2.c runtime/libia2/init.c runtime/libia2/threads.c
```

## Notes on certainty

Strongly evidenced by code and comments:

- Which pages are explicitly carved out/shared.
- Where startup/thread retagging happens.
- That `e664` adds mapping-level neighborhood retag and PKRU wrapper logic.

Inference (reasonable, but still inference):

- Exact minimal neighborhood size required for all workloads.
- Exact proportion of decode recovery attributable to each hunk vs interaction effects.
