# Dav1d Cross-Compartment Buffer Write: State So Far

Date: 2026-03-27

## 1) Executive Summary

The current decode crash is a **cross-compartment heap ownership mismatch**, not the original TLS/TCB issue.

- `ivf_read()` executes in compartment 1 and calls `fread(ptr, ...)`.
- `ptr` is returned by `dav1d_data_create()` from libdav1d (compartment 2), so it points to pkey-2 heap.
- `fread` writes while PKRU is compartment-1 style (`0xfffffff0`), so writing to pkey-2 memory faults with `SEGV_PKUERR`.

This is conceptually the same class of problem that IA2 solves with shared heap APIs (`shared_malloc`), but here it appears in the demux-read path.

## 2) Why Tools Is Compartment 1

This is by design, not incidental.

- `tools/dav1d.c` defines main compartment as 1:
  - `dav1d-ia2-ia2/tools/dav1d.c:3`
- Runtime assumes main compartment has pkey 1:
  - `runtime/libia2/main.c:8`
- `ia2_main()` registers `main -> 1`, `libdav1d.so -> 2`:
  - `dav1d-ia2-ia2/tools/dav1d.c:9-11`

Build config enforces this split:

- tools target uses `pkey = 1`, passes `-DPKEY=1`:
  - `dav1d-ia2-ia2/tools/meson.build:43`
  - `dav1d-ia2-ia2/tools/meson.build:49`
- libdav1d target uses `pkey = 2`, passes `-DPKEY=2`:
  - `dav1d-ia2-ia2/src/meson.build:318`
  - `dav1d-ia2-ia2/src/meson.build:323`

So `ivf_read` is pkey 1 because it lives in `tools/*` (compartment-1 build), even though it is in a different source file than `main`.

## 3) Why `ivf_read` Executes in Compartment 1

Dispatch path:

- `input_read()` calls `IA2_CALL(ctx->impl->read, ...)`:
  - `dav1d-ia2-ia2/tools/input/input.c:129`
- `ivf_demuxer.read = IA2_FN(ivf_read)`:
  - `dav1d-ia2-ia2/tools/input/ivf.c:200`

IA2 macro behavior:

- `IA2_CALL` uses caller `PKEY` in `_IA2_CALL(..., PKEY, ...)`:
  - `runtime/libia2/include/ia2.h:175`
- `_IA2_CALL` resolves to `__ia2_indirect_callgate_<sig>_pkey_<caller>`:
  - `runtime/libia2/include/ia2_internal.h:44-47`

Generated wrappers show pkey-1 indirect callgate for demux `read` signature:

- declaration exists:
  - `dav1d-ia2-ia2/callgate_wrapper.h:35`
- implementation exists:
  - `dav1d-ia2-ia2/callgate_wrapper.c:6-8`

## 4) The Failing Code Path

Core lines in `ivf_read`:

- alloc buffer from libdav1d API:
  - `ptr = dav1d_data_create(buf, sz);` at `dav1d-ia2-ia2/tools/input/ivf.c:159`
- read file data into that pointer:
  - `fread(ptr, sz, 1, c->f)` at `dav1d-ia2-ia2/tools/input/ivf.c:160`

`dav1d_data_create` is defined in libdav1d (compartment 2 DSO):

- `dav1d-ia2-ia2/src/lib.c:733-734`

Observed crash evidence (strict mode):

- `SEGV_PKUERR`, `si_addr=0x34ec00004020`
- destination mapping: `[anon:partition_alloc]`, `ProtectionKey: 2`
- PKRU at fault: `0xfffffff0` (compartment-1 style)
- stack includes: `__GI__IO_fread -> ivf_read -> __ia2_ivf_read -> callgate`
- source: `docs/conversation_repro_log_2026-03-27.md:288-293`

Interpretation: comp1 code attempted to write comp2-owned heap.

## 5) Relation To Libc-Compartment Behavior

With libc-compartment enabled, rewriter policy assigns system-header functions to compartment 1 (with exceptions like variadics):

- `tools/rewriter/SourceRewriter.cpp:1584-1589`
- `tools/rewriter/SourceRewriter.cpp:1602-1605`

So seeing `fread` execute under compartment-1 PKRU is expected in this configuration.

## 6) Is This The Same As TLS/TCB Crashes?

No. It is downstream of that work.

- Earlier failures were TLS/loader/libc runtime metadata visibility issues.
- Current failure is data ownership mismatch across compartments for a dynamic buffer.

In short: TLS carveouts got us past prior blockers; this crash is a different, later-stage issue.

## 7) Is There Already a Shared Heap Pattern in IA2?

Yes.

- Shared allocator API:
  - `runtime/partition-alloc/include/ia2_allocator.h:9-27`
- `shared_malloc` routes to `SharedAllocator()`:
  - `runtime/partition-alloc/src/shared_allocator.cc:6-10`
- Allocator uses per-compartment roots for normal allocation and a dedicated shared root for shared allocation:
  - `runtime/partition-alloc/src/allocator_shim_default_dispatch_to_partition_alloc.cc:183-200`

Dav1d docs also explicitly mention `shared_malloc` as the way to share data when stack/private ownership is a problem:

- `docs/compartmentalizing_dav1d.md:578-582`

## 8) Can We Change the Compartment Config?

Yes, but with constraints.

- You can reassign many source files/DSOs to different pkeys by changing build/rewriter config.
- You generally should **not** move `main` away from compartment 1 without runtime changes, because runtime assumes main is pkey 1 (`runtime/libia2/main.c:8`).

Changing the whole split is possible but large and likely to introduce new boundary issues.

## 9) Practical Fix Options for This Specific Fault

### Option A (preferred): Explicit shared packet buffer handoff

Make the packet payload buffer used by `fread` shared (via shared allocator path), or explicitly allocate read buffers from shared allocator at the demux boundary and then manage ownership carefully.

Why this is aligned with IA2 philosophy:

- Makes cross-compartment sharing explicit and narrow.
- Avoids broad page retagging or permissive-mode behavior.
- Reuses existing IA2 shared-heap primitive.

### Option B: Copy bridge between compartments

Read into compartment-1-owned buffer, then copy through a controlled bridge into compartment-2-owned destination (or vice versa depending contract).

Pros: precise boundary semantics.
Cons: extra copy overhead, more glue.

### Option C: Union-PKRU around specific operation

Temporarily allow both pkeys for this operation.

Pros: fast unblock.
Cons: weaker isolation, easy to over-broaden; less in line with strict policy.

### Option D: Broad retag/sharing of allocator pages

Least targeted and least desirable from isolation perspective.

## 10) Answers to the Most Recent Questions

- “Is problem create vs read?”
  - The faulting instruction is in the **write path during `fread`/`memmove`**, not in pointer creation itself.
- “Why comp1 write?”
  - Because `ivf_read` is a tools-compartment function (pkey 1) and libc-compartment policy routes libc/system calls to comp1.
- “Would removing callgate for `fread`/`ivf_read` solve it?”
  - Not by itself. The mismatch is ownership: comp1 writing comp2 buffer. Suppressing gates tends to move where you fail, not fix the contract.

## 11) Current Bottom Line

The immediate blocker is a **compartment contract issue for packet buffer memory**. The most principled next step is to introduce a **targeted shared-buffer or explicit bridge contract** at the demux/libdav1d boundary, rather than broadening global sharing or relying on permissive behavior.
