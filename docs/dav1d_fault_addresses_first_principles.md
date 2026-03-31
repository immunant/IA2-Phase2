# dav1d `--version` / Single-Thread Decode Fault Addresses and First-Principles Analysis

Date: 2026-03-26

Binary under test:
- `/home/davidanekstein/immunant/dav1d-ia2-ia2/build/x86_64/tools/dav1d`

Input under test:
- `/home/davidanekstein/immunant/test.ivf`

All fault captures below are from `gdb` on this binary with exact `si_addr`, faulting instruction, PKRU, and mapped pkey ownership.

## 1. What we knew before this pass

From earlier TP/TCB snapshots at `main`:
- `tp/fs_base = 0x7ffff7e91000`
- TP neighborhood maps:
  - `0x7ffff7e88000-0x7ffff7e90000` pkey 1
  - `0x7ffff7e90000-0x7ffff7e91000` pkey 2
  - `0x7ffff7e91000-0x7ffff7e96000` pkey 1

TCB header fields at TP confirmed glibc layout:
- `tp+0x00` tcb pointer, `tp+0x08` dtv, `tp+0x10` self, `tp+0x28` stack_guard.

## 2. Exact baseline failures (no manual retagging)

### A) `dav1d --version`

Fault site:
- RIP: `std::uncaught_exception()+14`
- Instruction: `mov 0x8(%rax), %eax`
- `si_code = 4` (`SEGV_PKUERR`)
- `si_addr = 0x7ffff7e90fe0`
- `pkru = 0xfffffff0` (IA2 compartment 1)

Fault address mapping:
- `0x7ffff7e90fe0` in `0x7ffff7e90000-0x7ffff7e91000` (pkey 2, `[anon: ia2-loader-heap]`)

Interpretation:
- `--version` teardown path in libstdc++ tried to read TLS state residing in a pkey 2 page while current PKRU was compartment 1.

### B) Single-thread decode

Fault site:
- RIP: `dav1d_ref_create+16`
- Instruction: `mov %fs:0x28, %rax`
- `si_code = 4` (`SEGV_PKUERR`)
- `si_addr = 0x7ffff7e91028` (TP + 0x28)
- `pkru = 0xffffffcc` (IA2 compartment 2)

Fault address mapping:
- `0x7ffff7e91028` in `0x7ffff7e91000-0x7ffff7e96000` (pkey 1)

Interpretation:
- Compartment 2 function prologue attempted stack-canary load from TCB (`%fs:0x28`) but TCB page was pkey 1.

## 3. Investigation by staged retagging (runtime-only experiments)

I used temporary `gdb` `pkey_mprotect` calls at `main` to see what fault appears next.

### Step 1: Share TP page only (`tp`)

Result:
- Decode no longer dies at `mov %fs:0x28`.
- Next failure in `__tls_get_addr` (`ld-linux`) at line 31.

### Step 2: Share `tp-1` and `tp`

Result:
- `--version` exits normally.
- Decode still fails in `__tls_get_addr`.

### Step 3: Share `tp-8..tp` window + `_rtld_local` page

Result:
- Next decode fault in `__tls_get_addr` at line 33.
- `si_addr = 0x7ffff7e92960` (DTV pointer location)
- Fault map: `0x7ffff7e92000-0x7ffff7e96000` (pkey 1)
- `pkru = 0xffffffcc` (compartment 2)

Interpretation:
- Partial TP-page retagging was insufficient; DTV metadata still sat in pkey 1.

### Step 4: Share full TP-adjacent maps + `_rtld_local` page

Result:
- Decode progressed past previous TLS faults.
- Next fault in libc `__memmove_evex_unaligned_erms` reading `__x86_rep_movsb_threshold`.
- `si_addr = 0x7ffff7a7e5b0`
- Fault map: `0x7ffff7a7d000-0x7ffff7a7f000` (`libc.so.6` writable data, pkey 1)
- `pkru = 0xffffffcc` (compartment 2)

Interpretation:
- Even after TP/TCB/TLS fixes, code running with compartment 2 PKRU still needs writable runtime globals from system libraries currently tagged pkey 1.

## 4. First-principles conclusion

The failures are not one isolated address; they are a class:
- TCB stack-guard reads (`%fs:0x28`)
- DTV / loader TLS metadata (`__tls_get_addr`, `_rtld_local`)
- libc writable runtime globals (`__x86_rep_movsb_threshold`, etc.)

All of these can be touched while PKRU is compartment 2.

### What this implies about the fix type

`Transition-only` is not sufficient by itself in current architecture:
- Many accesses are implicit inside uninstrumented runtime paths (`__tls_get_addr`, stack protector prologues, libc internals).
- There is no clean callgate boundary for every such access.

`Retag memory assignments` is required for ABI/runtime state that is truly process-runtime shared in practice.

## 5. Practical policy options (ordered by feasibility)

1. **Immediate unblock (current architecture):**
- Keep the full TP-overlap mappings shared (pkey 0), not just one page.
- Keep loader/libc runtime-writable state needed by cross-compartment execution shared, or otherwise accessible from all active compartments.

2. **More conservative but larger refactor:**
- Introduce a strict runtime/libc compartment entry model and ensure all runtime/library interactions (including TLS helper paths) transition into it.
- This likely requires additional compiler/rewriter/runtime control over implicit helper calls and may still need shared carveouts for unavoidable ABI fields.

3. **Avoid mixing compartment-private and runtime-critical TLS in same pages (long-term):**
- Relocate IA2-specific TLS control slots (e.g., stackptr slots) out of mixed runtime TLS pages where feasible.
- Then share only runtime-critical pages.

## 6. Current status

- Exact faulting addresses and owning pages are identified for `--version` and decode.
- The evidence points to a broader runtime-visibility problem, not a single bad pointer write.
- The shortest path to robustness is memory-policy based (shared runtime-critical regions), with transition-only as a longer-term architecture project.

## 7. Clean side-by-side (`main` vs `ad0606ff0`) using reproducible build flow

To avoid stale-artifact ambiguity, I rebuilt both IA2 and dav1d in clean directories:

- IA2 `main`: `/home/davidanekstein/immunant/approaches/ia2-cmp-main`
- IA2 `ad0606ff0`: `/home/davidanekstein/immunant/approaches/ia2-cmp-da`
- dav1d build dirs:
  - `/home/davidanekstein/immunant/dav1d-ia2-ia2/build/x86_64_cmp_main2`
  - `/home/davidanekstein/immunant/dav1d-ia2-ia2/build/x86_64_cmp_da2`

Critical reproducibility details (required):

1. Build `libcallgates.so` manually into `build/.../src` (`meson` links `-lcallgates` but does not auto-produce it here).
2. Run `pad-tls --allow-no-tls` on `tools/dav1d` and runtime DSOs after copying them into `build/.../src`.

If step 2 is skipped, both `--version` and decode can fail immediately in `__wrap_main` at `mov %fs:(%r11), %rsp`.

### Post-`pad-tls` behavior

- `--version`: succeeds on both `main` and `ad0606ff0`.
- Single-thread decode: crashes on both, but with different fault reasons.

#### `main` decode crash (post-`pad-tls`)

- Signal: `SEGV_PKUERR` (`si_code=4`)
- `si_addr=0x7ffff7e90028`
- RIP: `dav1d_ref_create+16`
- Instruction: `mov %fs:0x28, %rax`
- Fault map: `0x7ffff7e8d000-0x7ffff7e95000` (`[anon: ia2-loader-heap]`, `pkey=1`)
- `pkru=0xffffffcc` (compartment 2)

Interpretation:
- Compartment 2 decode path still faults on direct TCB stack-guard access (`%fs:0x28`) when the touched page is not accessible under current PKRU.

#### `ad0606ff0` decode crash (post-`pad-tls`)

- Signal: `SEGV_PKUERR` (`si_code=4`)
- `si_addr=0x7ffff7ffdaf0`
- RIP: `__tls_get_addr+13` (`tls_get_addr.S:31`)
- Fault map: `0x7ffff7ffd000-0x7ffff7ffe000` (`ld-linux-x86-64.so.2` writable page, `pkey=1`)
- `pkru=0xffffffcc` (compartment 2)

Interpretation:
- This path advances past the direct `%fs:0x28` failure and dies deeper inside dynamic TLS resolution (`ld-linux`), which is a distinct crash reason.

## 8. Why this distinction matters for fix design

- `main` decode failure is an immediate TCB/stack-guard visibility issue.
- `da` decode failure is dynamic TLS resolver/runtime metadata visibility (`__tls_get_addr`) under compartment 2.
- Therefore, a principled solution must cover both:
  - direct `%fs`-relative ABI accesses, and
  - resolver/runtime TLS metadata paths used implicitly by allocator/libc/loader code.

## 9. Exact Memory Targets And First-Principles Treatment

This section records one consistent capture style for each crash.

### A) `main` decode crash (`0eef8f8fa`) after full reproducible setup

Fault facts:
- `pkru=0xffffffcc` (compartment 2)
- `%fs_base=0x7ffff7e90000`
- RIP: `dav1d_ref_create+16`
- Instruction: `mov %fs:0x28,%rax`
- `si_addr=0x7ffff7e90028` (`fs_base + 0x28`)
- Fault map: `0x7ffff7e8d000-0x7ffff7e95000` (`[anon: ia2-loader-heap]`, `pkey=1`)

What that memory is:
- `%fs:0x28` is the x86_64 stack protector canary slot (`stack_guard`) in glibc TCB header (`tcbhead_t`).
- This is ABI/thread-runtime state, not compartment-private application data.

First-principles treatment:
- The page containing thread pointer ABI header fields must be accessible independent of active compartment.
- Practical policy: keep the TCB page shared (`pkey 0`) on x86_64.

### B) `ad0606ff0` decode crash after full reproducible setup

Fault facts:
- `pkru=0xffffffcc` (compartment 2)
- `%fs_base=0x7ffff7e90000` (already on `pkey=0` in this run)
- RIP: `__tls_get_addr+13` in `ld-linux`
- Instruction: `mov GL_TLS_GENERATION_OFFSET+_rtld_local(%rip), %rax`
- `_rtld_local = 0x7ffff7ffd000`
- `GL_TLS_GENERATION_OFFSET = 2800`
- `si_addr=0x7ffff7ffdaf0` (`_rtld_local + 2800`)
- Fault map: `0x7ffff7ffd000-0x7ffff7ffe000` (`ld-linux-x86-64.so.2` writable page, `pkey=1`)

What that memory is:
- Loader (`ld-linux`) process-global TLS bookkeeping (`_rtld_local`), read on fast-path dynamic TLS resolution (`__tls_get_addr`).
- This is runtime resolver metadata, not compartment-private decoder state.

First-principles treatment:
- Memory required by `__tls_get_addr` fast/slow paths must be accessible from any compartment that can execute code with dynamic TLS references.
- Practical policy: keep required loader TLS-metadata pages shared (`pkey 0`) or route all such accesses through a guaranteed runtime-compartment transition boundary (harder because calls are compiler/ABI implicit).

### C) Policy boundary (what should stay compartment-private)

- App TLS that is genuinely compartment-local should remain compartment-tagged.
- But page granularity means if ABI/runtime-critical data co-resides on a page with local TLS, that page must be treated as runtime-shared unless layout is redesigned.
