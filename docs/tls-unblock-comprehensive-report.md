# IA2 + dav1d TLS/PKRU Unblock Investigation (Comprehensive Report)

Date: 2026-03-22
Primary branch/workspace: `fix/shared-tls-tcb-rationale-docs` (`/home/davidanekstein/immunant/ia2`)
Primary active approach: `approaches/ia2-a` + `approaches/dav1d-a`

## 1) Goal
Reproduce and unblock the dav1d decode failure under IA2 until the historical TLS-related crash (`__tls_get_addr` / RTLD TLS access pattern) is passed, while preserving IA2 compartment-safety philosophy.

## 2) Current Outcome (As of 2026-03-22)
- `dav1d --version`: passes (`exit 0`) in Option A.
- Single-thread decode of `/home/davidanekstein/immunant/test.ivf`: passes (`exit 0`) in Option A.
- Multi-thread decode (`--threads 2`): still fails (currently `exit 139`, and with `IA2_PKRU_DIAG=1` exits via diagnostics).

## 3) How to Decode the Video (Reproducible Commands)

### 3.1 Runtime environment
```bash
export LD_LIBRARY_PATH=/home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/src:/home/davidanekstein/immunant/approaches/ia2-a/build/x86_64/runtime/libia2:/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu
```

### 3.2 Sanity check
```bash
/home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/tools/dav1d --version
```
Expected currently: prints `1.4.2-187-g39c386a` and exits `0`.

### 3.3 Decode (single-thread, currently working)
```bash
/home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/tools/dav1d -i /home/davidanekstein/immunant/test.ivf -o /dev/null
```
Expected currently: decodes 2/2 frames and exits `0`.

### 3.4 Decode (multi-thread, currently failing)
```bash
/home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/tools/dav1d --threads 2 -i /home/davidanekstein/immunant/test.ivf -o /dev/null
```
Expected currently: crashes (`exit 139` without diag mode).

## 4) Problems Encountered and Resolution Status

1. Problem: Meson path initially invalid in setup command.
Evidence: `.../bin/meson: No such file or directory`.
Resolution / Mitigation: Re-ran with valid Meson 1.10.1 path.
Status: Resolved.

2. Problem: Missing generated callgate wrapper files blocked dav1d link.
Evidence: Missing `callgate_wrapper.h`/related artifacts.
Resolution / Mitigation: Restored wrapper files and built `libcallgates.so`.
Status: Resolved.

3. Problem: Missing IVF repro file path.
Evidence: `/tmp/test.ivf` absent.
Resolution / Mitigation: Standardized on `/home/davidanekstein/immunant/test.ivf`.
Status: Resolved.

4. Problem: Early startup SIGSEGV in `__wrap_main`.
Evidence: gdb startup crash, `pkru=0xfffffff0`.
Resolution / Mitigation: Applied baseline TLS carve-out fixes and startup/runtime parity cleanup.
Status: Mitigated (moved crash later).

5. Problem: `--version` regression appeared in worktree runs.
Evidence: `--version` segfaults in A/B/C worktrees.
Resolution / Mitigation: Found IA2/dav1d config mismatch: IA2 `IA2_LIBC_COMPARTMENT=OFF` while dav1d `ia2_libc_compartment=true`; rebuilt IA2-A with `IA2_LIBC_COMPARTMENT=ON`.
Status: Resolved in A; explains artifact in B/C.

6. Problem: Stale runtime artifact in dav1d-a.
Evidence: Hash mismatch between copied `libpartition-alloc.so` and IA2-built version.
Resolution / Mitigation: Synced fresh `libpartition-alloc.so` into dav1d build tree.
Status: Resolved (artifact removed).

7. Problem: Decode crash with PKU fault in allocator TLS path.
Evidence: `SEGV_PKUERR`, fault in `libpartition-alloc.so+0x7d34b`, `allocation_guard.cc:18` (`thread_local bool g_disallow_allocations`).
Resolution / Mitigation: Expanded TLS carve-out logic; then generalized to retag static-TLS neighborhood around thread pointer.
Status: Partially resolved (single-thread passes).

8. Problem: PT_TLS-only carve-out was insufficient.
Evidence: Fault page remained outside currently retagged PT_TLS subrange.
Resolution / Mitigation: Added `/proc/self/maps`-based retagging near TCB (`ia2_unprotect_thread_pointer_mapping`).
Status: Resolved for single-thread path.

9. Problem: Multi-thread decode still crashes.
Evidence: `--threads 2` still SIGSEGV/diag fault; prior mapping pointed to libc memcpy path under PKRU mismatch conditions.
Resolution / Mitigation: Added thread-path adjustments (`ia2_thread_begin` retag call; temporary PKRU(0) around `pthread_create`).
Status: Unresolved.

## 5) Root Cause (Detailed and Illustrative)

### 5.1 First-principles model
- PKRU is a per-thread register that controls access rights to pages by protection key (pkey).
- IA2 assigns different pkeys to different memory regions/compartments.
- TLS variables are not special metadata; they are ordinary memory locations inside each thread's TLS storage region.
- `__tls_get_addr` computes/returns where a TLS variable lives, but the returned address is only usable if the page containing it is accessible under current PKRU settings.
- Therefore, even if TLS address resolution is correct, access still fails if page tagging and PKRU policy are mismatched.

### 5.2 Observed failing path
- Repeated crash site: `libpartition-alloc.so+0x7d34b`, mapping to allocator TLS guard access (`allocation_guard.cc:18`, `thread_local bool g_disallow_allocations`).
- In failing traces, Linux reported `SEGV_PKUERR` with `si_pkey=1`.
- The faulting address fell in an `ia2-loader-heap` mapping that remained tagged `pkey=1`.
- Nearby pages were retagged to shared `pkey=0`, but not the exact page used by this TLS object in that run.
- Result: read/write to allocator TLS guard trapped even though code path and pointer arithmetic were otherwise valid.

### 5.3 Why this originally looked like an `__tls_get_addr` issue
- TLS failures naturally appear near loader/TLS resolution code (`rtld` + `__tls_get_addr`), so the symptom can look like a resolver bug.
- The stronger evidence points elsewhere: once wider correct retagging was applied, the same classes of accesses succeeded without changing TLS lookup semantics.
- This indicates the primary fault is access-domain mismatch on TLS backing pages, not the local `res` variable or fundamental resolver logic.

### 5.4 Concrete illustration
```text
Thread execution context:
  %fs --> TCB --> static TLS block --> g_disallow_allocations (thread_local)

Failing state:
  page(g_disallow_allocations) tagged pkey=1
  current PKRU context does not allow that pkey for this access path

Outcome:
  TLS load/store faults with SIGSEGV (SEGV_PKUERR)
```

## 6) Approaches Tried (A/B/C)

### Option A (IA2-aligned)
- Strategy: keep compartment model strict; fix TLS sharing/retag behavior narrowly.
- Key changes:
  - widened TLS carve-out policy in `runtime/libia2/ia2.c`
  - added thread-pointer-neighborhood retagging via `/proc/self/maps`
  - invoked this in startup and thread entry paths
- Result:
  - `--version`: pass
  - single-thread decode: pass
  - multi-thread decode: still failing

### Option B (broader allocator allow-all scope)
- Strategy: temporarily widen PKRU at allocator entry points (`Wrpkru(0)` scoped guards).
- Result: startup instability remained in tested state, and policy is weaker than A.
- Note: B runs also had IA2/dav1d libc-compartment config mismatch artifact, confounding interpretation.

### Option C (union-PKRU guard)
- Strategy: narrower relaxation than B (union mask rather than full allow-all).
- Result: similar startup instability in tested state; still policy relaxation relative to A.
- Note: C also had the same config mismatch artifact in recorded runs.

## 7) Fix Details: What Was Changed, How, and Why It Helps

### 7.1 Fix step A: remove confounding artifacts first
- Aligned IA2/dav1d config parity for libc compartmenting:
  - IA2 rebuilt with `IA2_LIBC_COMPARTMENT=ON` to match dav1d `ia2_libc_compartment=true`.
- Ensured `dav1d-a/build/.../libpartition-alloc.so` was synchronized with the current IA2 build output.
- Evaluated partition-alloc TLS model override necessity:
  - A/B tested `initial-exec` versus `global-dynamic`.
  - In current Option A state, both `dav1d --version` and single-thread decode succeeded in both configurations.
  - Based on that evidence, `-ftls-model=initial-exec` is treated as optional and not required for current baseline invocation success.
- Why this matters:
  - eliminated false startup failures and stale-runtime behavior so TLS root-cause signals were reliable.

### 7.2 Fix step B: widen TLS carve-out logic in IA2 startup
- File: `runtime/libia2/ia2.c`
- Change:
  - expanded shared TLS page carve-out policy beyond a very small fixed set.
  - included stack-pointer TLS pages and TCB-adjacent carve-outs with ordering before protection application.
- Why this helps:
  - reduces chance that legitimate cross-compartment TLS accesses land on unshared pages.
- Limitation observed:
  - PT_TLS-window-only handling still missed some real fault pages in practice.

### 7.3 Fix step C: add thread-pointer neighborhood retagging
- Files:
  - `runtime/libia2/ia2.c` (new `ia2_unprotect_thread_pointer_mapping()`)
  - `runtime/libia2/include/ia2_internal.h` (prototype)
  - `runtime/libia2/init.c` (call from `ia2_start()`)
- Change:
  - parse `/proc/self/maps`, locate RW mappings around the active thread pointer/TCB neighborhood, retag relevant pages to shared `pkey=0` via `ia2_mprotect_with_tag`.
- Why this helps:
  - directly covers the real static-TLS placement region seen in failures, including cases outside the narrower module PT_TLS carve-out window.
- Observed effect:
  - single-thread decode moved from crashing to stable success (`exit 0`).

### 7.4 Fix step D: extend thread-path handling
- File: `runtime/libia2/threads.c`
- Changes:
  - call `ia2_unprotect_thread_pointer_mapping()` in `ia2_thread_begin()` after TLS setup.
  - temporarily set `PKRU(0)` around `__real_pthread_create`, then restore.
- Why this helps:
  - attempts to bridge the vulnerable period where new-thread TLS and runtime transitions may touch pages before final stable policy is re-established.
- Current outcome:
  - improved coverage, but multi-thread decode still crashes, so more thread-path refinement is required.

### 7.5 Why this is a legitimate fix direction
- It preserves IA2's compartment model better than broad always-open PKRU workarounds.
- It targets memory-domain correctness where the data actually resides (TLS backing pages), instead of globally flattening protections.
- It explains the observed behavior split:
  - single-thread path now works because its TLS access region is properly shared/retagged;
  - multi-thread path still fails because a remaining thread-transition access window is not fully covered.

## 8) Remaining Open Problem
Multi-thread decode still fails, which indicates a remaining TLS/PKRU transition gap in thread creation/startup/runtime memcpy/access paths. The unresolved area is not startup `--version` anymore; it is thread-path correctness under compartmented PKRU policy.

## 9) Files and Logs to Consult
- Consolidated A/B/C summary: `approaches/ia2-a/docs/tls-unblock-three-approaches.md`
- Append-only logs:
  - `approaches/ia2-a/docs/tls-unblock-approach-log.md`
  - `approaches/ia2-b/docs/tls-unblock-approach-log.md`
  - `approaches/ia2-c/docs/tls-unblock-approach-log.md`
- Option A runtime edits currently carrying the working single-thread behavior:
  - `approaches/ia2-a/runtime/libia2/ia2.c`
  - `approaches/ia2-a/runtime/libia2/init.c`
  - `approaches/ia2-a/runtime/libia2/threads.c`
  - `approaches/ia2-a/runtime/libia2/include/ia2_internal.h`

## 10) Practical Takeaway
The investigation progressed past the original TLS-get-addr style blocker for real decode work in single-thread mode without globally flattening protections. The next blocker is now specifically multi-thread TLS/PKRU boundary behavior.
