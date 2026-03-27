# Conversation Repro Log (2026-03-27)

Purpose: concise, reproducible record of what we validated in this thread.

## Scope

- Focused commits:
1. `0eef8f8fafc19b6d3a31bf5943943bd522254b67` (`main`)
2. `da780290f6a2c2f122263a0650f30f2580babaa7`
3. `6943634e87d99d27136b3b1e1ef7e6a3682ecee0` (targeted TP-page change)
4. `e6641c15eaab4017c78e44bc305ce56be1bdf2d4`
5. `6c41d8f909bb672f1c7de56bb4f4f78918cd68fb` (targeted+e664 branch commit)

- Runtime mode:
1. `ia2_permissive_mode=false`
2. `ia2_libc_compartment=true`
3. Post-`pad-tls` flow applied (including `tools/dav1d`, `libcallgates.so`, `libdav1d.so`, runtime DSOs).

## Build/Run Preconditions

For each build:

1. Build `libcallgates.so` into `<dav_build>/src`.
2. Copy DSOs into `<dav_build>/src`:
`libpartition-alloc.so`, `libc.so.6`, `libm.so.6`, `libstdc++.so.6`, `libgcc_s.so.1`.
3. Run `pad-tls --allow-no-tls` on:
`tools/dav1d`, `src/libcallgates.so`, `src/libdav1d.so.7.0.0`, and copied DSOs.
4. Run:
`tools/dav1d --version`
`tools/dav1d -i /home/davidanekstein/immunant/test.ivf -o /dev/null --muxer null --threads 1`

## Verified Build Dirs Used

1. `main` -> `/home/davidanekstein/immunant/dav1d-ia2-ia2/build/x86_64_cmp_main2`
2. `da780` -> `/home/davidanekstein/immunant/dav1d-ia2-ia2/build/x86_64_cmp_da2`
3. `694` -> `/home/davidanekstein/immunant/dav1d-ia2-ia2/build/x86_64_verify_694_onbuild`
4. `e664` -> `/home/davidanekstein/immunant/dav1d-ia2-ia2/build/x86_64_verify_e664_onbuild`
5. `6c41` -> `/home/davidanekstein/immunant/dav1d-ia2-ia2/build/x86_64_verify_6c41_strict_fresh`

## Current Results

### `--version` (strict)

All 5 commits returned `rc=0` and printed `1.4.2-187-g39c386a`.

### Single-thread decode (strict)

All 5 commits returned `rc=139` (segfault), with two failure families:

1. `main` (`0eef8f8fa`):
- `RIP=dav1d_ref_create+16`
- instruction `mov %fs:0x28,%rax`
- `si_addr=0x7ffff7e90028`
- `pkru=0xffffffcc`

2. `da780`, `694`, `e664`, `6c41`:
- `RIP=__tls_get_addr+13` (`tls_get_addr.S:31`)
- `si_addr=0x7ffff7ffdaf0`
- `pkru=0xffffffcc`

## Deep Dive: Commit `6943634e8` Decode Failure

Observed fault:

1. `si_code=4 (SEGV_PKUERR)`
2. `si_addr=0x7ffff7ffdaf0`
3. `RIP=__tls_get_addr+13`
4. faulting read targets `_rtld_local+0xaf0` (`GL_TLS_GENERATION_OFFSET` path).

Memory mapping at fault:

1. `0x7ffff7ffd000-0x7ffff7ffe000`
2. perms `rw-p`
3. `pkey=1`
4. file: `.../runtime/libia2/ld-linux-x86-64.so.2`
5. offset `0x36000`, fault delta `0xaf0`.

Related mappings:

1. `fs_base=0x7ffff7e90000` in `[anon: ia2-loader-heap]`, `pkey=0`.
2. `RIP` in ld-linux text mapping, `pkey=0`.

PKRU decode at crash (`0xffffffcc`):

1. `pkey0` allowed
2. `pkey1` denied
3. `pkey2` allowed
4. `pkey3` denied

Interpretation:

1. Execution context is effectively compartment-2 style access policy (pkey2 active).
2. `__tls_get_addr` attempts to read loader metadata on a pkey1 page.
3. This pkey mismatch causes the decode crash.

## Minimal Repro Commands (Per Built Dir)

```bash
# version
<BUILD>/tools/dav1d --version

# decode
<BUILD>/tools/dav1d -i /home/davidanekstein/immunant/test.ivf -o /dev/null --muxer null --threads 1
```

```bash
# quick gdb signature
gdb -q <BUILD>/tools/dav1d <<'GDB'
set pagination off
set args -i /home/davidanekstein/immunant/test.ivf -o /dev/null --muxer null --threads 1
run
info registers rip pkru fs_base
bt 6
quit
GDB
```

## Append-Only Log Entries

- 2026-03-27: Verified strict `--version` and strict single-thread decode across `main`, `da780`, `694`, `e664`, `6c41`.
- 2026-03-27: Deep-mapped `694` decode fault to ld-linux writable page (`_rtld_local+0xaf0`, `pkey=1`) with active PKRU denying pkey1.
- 2026-03-27: Documented how to locate exact fault pages (`/proc/<pid>/{maps,smaps}`, `fs_base` via `ARCH_GET_FS`) and when addresses are determined (startup/thread-create/runtime update phases).
- 2026-03-27: Re-ran strict `694` decode under gdb and captured authoritative stack for the `_rtld_local+0xaf0` fault (`__tls_get_addr+13`), including the active IA2 indirect callgate frame.
- 2026-03-27: Implemented targeted ld.so metadata carveout in `protect_pages()` (exact page used by `__tls_get_addr` generation read) and validated in fresh strict build `x86_64_ia2_current_ldso_carveout`: `--version rc=0`, decode still `rc=139`.
- 2026-03-27: Post-ld.so carveout decode fault moved from `__tls_get_addr+13` to `__tls_get_addr+16` (`cmp %rax,(%rdx)`), `si_addr=0x7ffff7e30960` (DTV pointer read path).
- 2026-03-27: Tried PT_TLS-local DTV page carveout; no effect for this case because faulting DTV page was not carved by that path in this runtime layout.
- 2026-03-27: Added explicit startup DTV-page unprotect (`fs:8` page) and validated in fresh strict build `x86_64_ia2_current_ldso_dtv_unprotect`: `--version rc=0`, decode still `rc=139`.
- 2026-03-27: Latest decode fault after DTV unprotect is past `__tls_get_addr`, in `partition_alloc::ScopedDisallowAllocations` `testb $0x1,(%rax)` with `si_addr=0x7ffff7e28fe0` on `[anon: ia2-loader-heap]` `ProtectionKey: 1` under `PKRU=0xffffffcc`.
- 2026-03-27: Added startup carveout for writable `[anon: ia2-loader-heap]` mappings and validated in fresh strict build `x86_64_ia2_current_ldso_loaderheap`: `--version rc=0`, decode still `rc=139`.
- 2026-03-27: After loader-heap carveout, decode fault moved to libc memmove path (`__memmove_evex_unaligned_erms`) reading `__x86_rep_movsb_threshold` (`si_addr=0x7ffff7b0e5b0`) under `PKRU=0xffffffcc` (pkey1 denied).

## Authoritative Stack (694 strict decode, `_rtld_local` fault)

Captured from:

`gdb -q /home/davidanekstein/immunant/dav1d-ia2-ia2/build/x86_64_verify_694_onbuild/tools/dav1d -x /tmp/gdb_694_callstack.cmds`

Top frames:

1. `#0 __tls_get_addr` (`tls_get_addr.S:31`)  
   Faulting read: `_rtld_local+0xaf0` (`GL_TLS_GENERATION_OFFSET` path)
2. `#1 partition_alloc::ScopedDisallowAllocations::ScopedDisallowAllocations()`
3. `#2 allocator_shim::internal::PartitionMemalign(...)`
4. `#3 ShimMemalign`
5. `#4 ShimPosixMemalign`
6. `#5 __wrap_posix_memalign`
7. `#6 dav1d_alloc_aligned_internal` (`src/mem.h:95`)
8. `#7 dav1d_ref_create` (`src/ref.c:40`)
9. `#8 dav1d_data_create_internal` (`src/data.c:47`)
10. `#9 dav1d_data_create` (`src/lib.c:734`)
11. `#10 __wrap_dav1d_data_create` (`libcallgates.so`)
12. `#11 ivf_read` (`tools/input/ivf.c:159`)
13. `#12 __ia2_ivf_read` (`tools/input/ivf.c:192`)
14. `#13 __ia2_indirect_callgate...pkey_1` (`libcallgates.so`)

Signal/register highlights at fault:

- `RIP=__tls_get_addr+13`
- `PKRU=0xffffffcc` (pkey1 denied, pkey2 allowed)
- `FS_BASE=0x7ffff7e90000`

Implication:

- We already crossed an IA2 callgate (frame #13) before entering libdav1d/allocator code.
- Fault occurs in implicit glibc TLS resolver path while running with compartment-2 style PKRU.
- Adding a new callgate immediately before this specific memory touch is not generally practical because this access is inside libc/ld.so TLS machinery (`__tls_get_addr`) reached from compiler/runtime-generated TLS access sequences, not a stable explicit app API boundary.

## Proposed Targeted Carveout Method (Step-by-Step)

Goal:

- Keep IA2 default policy (`PT_TLS` pages compartment-tagged) while sharing only ABI-critical pages required by implicit TLS/runtime machinery.

Step 1: Keep default TLS tagging model unchanged

- In `protect_tls_pages()`, continue tagging module TLS ranges to target compartment pkey by default.
- Do not introduce broad TP windows or map-wide retagging.

Step 2: Build a deduplicated carveout page set per module TLS range

- For each `PT_TLS` range (`start_round_down..end`), compute candidate shared pages and add only overlapping ones:
1. `ia2_stackptr_0` page (existing callgate stack slot behavior).
2. Current-thread TCB page (`page(__builtin_thread_pointer())`).
3. Loader TLS-generation page (`page(&_rtld_local + GL_TLS_GENERATION_OFFSET)`), only if that page falls in a writable `ld-linux` mapping and is proven fault-relevant.

Step 3: Protect non-carveout subranges with compartment pkey

- Sort carveout pages and walk with a cursor:
1. Retag `[cursor, carveout_page)` -> compartment pkey.
2. Skip carveout page.
3. Continue until `end`.

Step 4: Retag carveout pages to shared pkey 0 during process init only

- Apply `pkey_mprotect(..., pkey=0)` for carveout pages when `ia2_get_compartment() == 0`.
- Skip redundant post-init retagging from nonzero compartments.

Step 5: Explicitly re-assert TP/TCB page sharing after compartment setup

- Keep/init `ia2_unprotect_thread_pointer_page()` after `ia2_setup_compartment()` loop in `init.c`.
- This guarantees `%fs` ABI state remains accessible regardless of current compartment PKRU.

Step 6: Ensure per-thread correctness

- For thread start path, ensure each new thread executes TLS setup and TP-page unprotect sequence before compartment-crossing work.
- Treat TCB carveout as per-thread (address differs per thread).

Step 7: Optional loader metadata carveout is evidence-gated

- Do not blanket-share all loader writable pages.
- Only share exact faulted ABI page(s) proven necessary (currently `_rtld_local + GL_TLS_GENERATION_OFFSET` page for this failure).

Step 8: Validate in strict mode only

- `ia2_permissive_mode=false` always.
- Re-run both:
1. `tools/dav1d --version`
2. single-thread decode (`--threads 1`)
- Collect gdb + `smaps` proof for any new `SEGV_PKUERR` and add only minimal additional carveouts if needed.

Step 9: Add guardrails

- Runtime assert/log in debug builds when `__tls_get_addr`-related fault address lands outside known carveout set.
- Keep carveout count bounded and deterministic.

Step 10: Security posture

- Prefer smallest-page-set sharing (pkey0) over union-PKRU between compartments.
- Union-PKRU is fallback only if repeated, unavoidable ABI paths cannot be page-targeted safely.

## Runtime Address Determination (External References)

How to get exact pages at runtime:

1. Fault address -> mapping:
- Read `/proc/<pid>/maps` for mapping boundaries.
- Read `/proc/<pid>/smaps` for per-mapping metadata, including `ProtectionKey`.
2. TCB page:
- Read thread-pointer base (`%fs`) with `arch_prctl(ARCH_GET_FS, ...)`.
- Compute page as `fs_base & ~(PAGE_SIZE - 1)`.
3. TLS resolver metadata page:
- In x86_64 glibc, `__tls_get_addr` reads `GL_TLS_GENERATION_OFFSET+_rtld_local`.
- Resolve `_rtld_local` (symbols/debugger), add offset, page-align down.

When addresses/contents are determined:

1. Process startup:
- Kernel starts ELF interpreter from `PT_INTERP`; dynamic linker maps initial objects (`ld-linux`, libc, app DSOs).
- Absolute addresses are chosen at runtime (ASLR), so they vary across runs.
2. Thread creation:
- Each thread gets its own TP/TCB/TLS placement; `%fs` is set with `ARCH_SET_FS`.
3. Runtime evolution:
- Loader TLS bookkeeping (e.g., `dl_tls_generation`) changes over time (e.g., module load/update paths), while mapping locations usually remain stable unless new mappings are created.

References used:

- `proc_pid_maps(5)`: https://man7.org/linux/man-pages/man5/proc_pid_maps.5.html
- `proc_pid_smaps(5)`: https://man7.org/linux/man-pages/man5/proc_pid_smaps.5.html
- `arch_prctl(2)`: https://man7.org/linux/man-pages/man2/arch_prctl.2.html
- `ld.so(8)`: https://man7.org/linux/man-pages/man8/ld-linux.8.html
- `dlinfo(3)` / link-map visibility: https://man7.org/linux/man-pages/man3/dlinfo.3.html
- glibc x86_64 TLS/TP setup (`tls.h`): https://codebrowser.dev/glibc/glibc/sysdeps/x86_64/nptl/tls.h.html
- glibc `__tls_get_addr` (`tls_get_addr.S`): https://codebrowser.dev/glibc/glibc/sysdeps/x86_64/tls_get_addr.S.html
- glibc TLS generation field (`ldsodefs.h`): https://codebrowser.dev/glibc/glibc/sysdeps/generic/ldsodefs.h.html
- glibc TLS update path (`dl-tls.c`): https://codebrowser.dev/glibc/glibc/elf/dl-tls.c.html
- Linux pkeys overview (`pkeys(7)`): https://man7.org/linux/man-pages/man7/pkeys.7.html
- Linux kernel ASLR/sysctl docs:
  - `randomize_va_space`: https://www.kernel.org/doc/html/latest/admin-guide/sysctl/kernel.html
  - `mmap_rnd_bits`: https://docs.kernel.org/6.15/admin-guide/sysctl/vm.html

## Append-Only Log Entries (continued)

- 2026-03-27: Re-ran strict decode in `x86_64_ia2_current_ldso_loaderheap` under gdb and resolved current `si_addr=0x7ffff7b0e5b0` to libc symbol `__x86_rep_movsb_threshold` (`.data`), faulting from `__memmove_evex_unaligned_erms` with `PKRU=0xffffffcc`.
- 2026-03-27: Verified owning mapping for `0x7ffff7b0e5b0` via `/proc/<pid>/smaps`: `7ffff7b0d000-7ffff7b0f000 rw-p ... src/libc.so.6`, `ProtectionKey: 1`.
- 2026-03-27: Confirmed first argument storage for `posix_memalign` in this path (`&ptr` in `dav1d_alloc_aligned_internal`) is on compartment-2 stack mapping (`pkey=2`), so current crash is not a write to that output pointer.
- 2026-03-27: Classified faulting memory semantics:
  - `__x86_rep_movsb_threshold`: address chosen at load time (ASLR), value initialized from CPU/tunable path during process startup (`sysdeps/x86/cacheinfo.h`, `sysdeps/x86/cpu-features.c`).
  - `_rtld_local + GL_TLS_GENERATION_OFFSET`: address chosen at load time, value can change at runtime on TLS generation updates (`elf/dl-open.c`, `elf/dl-close.c`).
- 2026-03-27: Implemented targeted libc tuning carveout in `protect_pages()` for `is_libc && syslib`: resolve `__x86_rep_movsb_threshold`, `__x86_rep_stosb_threshold`, and `__x86_rep_movsb_stop_threshold` from libc ELF symbol table (supports `SHT_DYNSYM` and `SHT_SYMTAB`), then mark only those symbol pages shared (`pkey 0`) if they land in writable libc LOAD segments.
- 2026-03-27: Found and fixed a reproducibility pitfall while testing: copying host `/lib64/ld-linux-x86-64.so.2` over IA2 runtime loader path causes early startup `SIGSEGV` (null maperr in loader path). Restoring `build/x86_64/external/glibc/sysroot/lib/ld-linux-x86-64.so.2` to `build/x86_64/runtime/libia2/ld-linux-x86-64.so.2` recovers normal startup.
- 2026-03-27: Validation on fresh strict build `x86_64_carveout_memmove_strict_20260327_2`: `--version rc=0`; single-thread decode still `rc=139` but progressed past prior threshold-read fault.
- 2026-03-27: New decode fault signature after libc tuning carveout:
  - RIP: `__memmove_evex_unaligned_erms+479` (`VMOVA %VMM(1), (%rdi)` write path)
  - PKRU: `0xfffffff0`
  - `si_addr`: non-canonical `0xc000004020` (observed variant `0x223000004020` in prior run)
  - Stack: `__GI__IO_fread -> ivf_read -> __ia2_ivf_read -> callgate`
  - Interpretation: old pkey-denied read of libc threshold global is bypassed; next blocker is a later invalid-destination memmove/fread path.
- 2026-03-27: Refined mapping evidence for the new `fread/memmove` write fault:
  - `si_code=4 (SEGV_PKUERR)`, `si_addr=0x34ec00004020`.
  - Faulting destination mapping: `34ec00004000-34ec00008000 rw-p [anon:partition_alloc]`, `ProtectionKey: 2`.
  - Active PKRU at fault: `0xfffffff0` (compartment-1 style; pkey2 denied).
  - `ivf_read` local `ptr` equals `0x34ec00004000` and is passed to `fread`.
  - Interpretation: this is a compartment mismatch on dynamic heap memory (partition-alloc-owned pkey2 buffer written while executing with compartment-1 PKRU), not TLS/TCB/loader ABI metadata.
- 2026-03-27: Added consolidated state report `docs/dav1d_compartment_mismatch_writeup.md` summarizing: compartment mapping rationale (tools pkey1, libdav1d pkey2), `IA2_CALL`/indirect-callgate execution mechanics, current `ivf_read` + `fread` cross-compartment write fault evidence (`pkey2` destination under `PKRU=0xfffffff0`), distinction from prior TLS/TCB faults, and targeted fix options (shared-buffer handoff preferred).
- 2026-03-27: Added `docs/dav1d_fixes_so_far_status_2026-03-27.md` to separate committed fix chain (`main..da780`) from current uncommitted runtime experiments (DTV/loader-heap/libc-symbol carveouts and test wiring), and to answer commit-status ambiguity.
- 2026-03-27: Snapshotted current runtime+docs investigation to branch `fix/dav1d-carveout-followups` at commit `4889607ff8ced61038ea24137823dfefd9c20198` (includes runtime carveout experiments, reproducibility docs, memory-map analysis docs, and `tests/tls_one_page_repro` scaffold).
