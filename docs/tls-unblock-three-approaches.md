# TLS Unblock Investigation: Three Approaches (A/B/C)

Date: 2026-03-22

## Scope
This document consolidates the three mitigation approaches attempted in isolated worktrees to move past IA2 TLS/PKRU failures while decoding AV1 with `dav1d`.

Related per-approach append-only logs:
- `ia2-a/docs/tls-unblock-approach-log.md`
- `ia2-b/docs/tls-unblock-approach-log.md`
- `ia2-c/docs/tls-unblock-approach-log.md`

## Baseline Repro Command
```bash
env LD_LIBRARY_PATH=<dav1d-build>/src:<ia2-build>/runtime/libia2:/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu \
  <dav1d-build>/tools/dav1d -i /home/davidanekstein/immunant/test.ivf -o /dev/null
```

## Shared Foundation Applied Across Approaches
One common change was applied first to unblock the initial `__tls_get_addr` path:

1. TLS carve-out widening in `runtime/libia2/ia2.c`
- Expanded shared TLS page handling from fixed `2` pages to `IA2_MAX_COMPARTMENTS + 2`.
- Carved out all active `ia2_stackptr_N` pages that overlap module TLS.
- Preserved TCB page carve-out.
- Sorted carve-outs before region protection.

Historical note:
- We also tested `-ftls-model=initial-exec` for partition-alloc on x86_64.
- Controlled A/B in Option A (`initial-exec` vs `global-dynamic`) showed no dependency for `dav1d --version` or single-thread decode success in the current state.

## Important Build/Config Finding (Artifact Root Cause)
A worktree-only regression in `dav1d --version` was caused by config mismatch:
- `dav1d`: `ia2_libc_compartment=true`
- IA2 worktrees (A/B/C initially): `IA2_LIBC_COMPARTMENT=OFF`

This produced inconsistent callgate/wrapper PKRU behavior in `__wrap_main`/`exit` and created a false `--version` failure surface.

After aligning Option A to `IA2_LIBC_COMPARTMENT=ON`, `--version` returned to success (`exit 0`).

## Approach A (Most IA2-aligned / Safer)
### Intent
Preserve strict compartment policy; avoid broad PKRU widening. Use targeted TLS correctness fixes only.

### Changes
- `runtime/libia2/ia2.c` (shared TLS carve-out logic as above)
- `runtime/partition-alloc/CMakeLists.txt` was previously experimented with for TLS model tuning; this is not required for current `--version`/single-thread success.

### Results
- Before config parity fix: `--version` failures were observed (artifact).
- After enabling IA2 libc compartment (`IA2_LIBC_COMPARTMENT=ON`) and rebuilding:
  - `dav1d --version`: success (`exit 0`)
  - decode command: still SIGSEGV (`exit 139`)

### Assessment
Best match to IA2 design goals. Remaining crash appears to be a real decode-path issue, not a startup artifact.

## Approach B (Middle Ground / Less Safe)
### Intent
Temporarily widen PKRU inside allocator entry points to test allocator-boundary hypothesis.

### Changes
In addition to shared foundation:
- `runtime/partition-alloc/src/allocator_shim_default_dispatch_to_partition_alloc.cc`
  - added scoped guard that sets `Wrpkru(0)` for shim entry points, restores previous PKRU on exit.
- `runtime/partition-alloc/src/shared_allocator.cc`
  - same allow-all guard for shared allocator exports.

### Results
- Encountered startup/runtime instability and `--version` failures before config parity correction.
- Not promoted due to broad temporary access and weaker isolation stance.

### Assessment
Useful as diagnostic pressure-test; not preferred as final direction.

## Approach C (Wider Union PKRU / Least Safe of Three)
### Intent
Allow union of allocator pkey and pkey0 rather than full allow-all, still broader than A.

### Changes
In addition to shared foundation:
- `runtime/partition-alloc/src/allocator_shim_default_dispatch_to_partition_alloc.cc`
  - scoped union-mask PKRU policy around shim calls.
- `runtime/partition-alloc/src/shared_allocator.cc`
  - same union-mask policy around shared allocator calls.

### Results
- Similar instability patterns to B before config parity correction.
- Still a policy relaxation versus A.

### Assessment
Narrower than B but still not ideal for long-term IA2 policy integrity.

## Current Recommendation
Proceed with Approach A as primary track, with strict config parity:
- If `dav1d` uses `ia2_libc_compartment=true`, IA2 must be built with `IA2_LIBC_COMPARTMENT=ON`.
- Keep B/C as diagnostics only.

## Current Status (Option A)
- Startup artifact resolved.
- Single-thread decode path now succeeds on `test.ivf` (`exit 0`) after widening shared retagging around the `%fs`/TCB static-TLS neighborhood.
- Multi-thread decode (`--threads 2`) still crashes (`exit 139`), indicating remaining thread-start/runtime TLS-PKRU issues not yet resolved by current Option A changes.
