# dav1d Main-Style Experiment Summary (2026-03-27)

## Goal
Try more principled/main-style alternatives to broad pthread wrapper union policy, while keeping strict-mode single-thread decode working.

## Baseline
- Build: `/home/davidanekstein/immunant/dav1d-ia2-ia2/build/x86_64_ab_shared_clean_20260327_1`
- Strict results: `--version rc=0`, decode (`--threads 1`) `rc=0`.

## Experiment A: shared-object contracts + pkey1-only pthread wrappers
Changes attempted:
- In dav1d source:
  - `pthread_once_t` locals in `src/lib.c`/`src/scan.c` moved to `IA2_SHARED_DATA`.
  - `pthread_attr_t` in `dav1d_open` moved to shared heap (`shared_malloc/shared_free`).
  - `Dav1dMemPool.lock` changed from inline `pthread_mutex_t` to shared `pthread_mutex_t *`.
- In `callgate_wrapper.c`:
  - forced `__wrap_pthread_*` wrappers to pkey1-only (`0xfffffff0`) in the pthread block.

Outcome:
- Strict `--version rc=0`.
- Strict decode `rc=139`.
- First fault: `dav1d_init_cpu` via `pthread_once` callback under pkey1-only PKRU (`0xfffffff0`).

## Follow-up A1
- Set only `__wrap_pthread_once` back to union (`0xffffffc0`) while other pthread wrappers stayed pkey1-only.

Outcome:
- Decode still failed (`rc=139`) at `__wrap_pthread_attr_init`.
- Disassembly showed fault at wrapper return-path `push %rax` after restoring caller stack pointer while still in pkey1-only mode.
- Conclusion: wrapper transition/stack-handoff phases require caller-memory visibility.

## Experiment B: tighter wrapper policy (worked)
Policy applied in `callgate_wrapper.c` pthread wrapper block:
- Transition phases (1st and 3rd PKRU write): union (`0xffffffc0`).
- libc call phase (middle PKRU write): pkey1-only (`0xfffffff0`).
- Exception: `__wrap_pthread_once` kept union through call.

Kept shared-object changes from Experiment A.

Outcome (strict):
- Build: `/home/davidanekstein/immunant/dav1d-ia2-ia2/build/x86_64_mainstyle_targeted_obj_strict_20260327_1`
- `--version rc=0`
- decode (`--threads 1`) `rc=0` (`Decoded 2/2`)

## What this means
- We can tighten from “union at all pthread call sites” to a more scoped policy:
  - union for wrapper transitions,
  - pkey1 for most libc pthread call bodies,
  - explicit exception for `pthread_once` callback behavior.
- Explicit shared-object placement (`IA2_SHARED_DATA`, `shared_malloc`) remains useful and aligned with main-style explicit sharing contracts.

## Commit status
- No new commit was made in this step.
- Work is currently uncommitted (plus pre-existing local modifications) in:
  - `/home/davidanekstein/immunant/dav1d-ia2-ia2`
  - `/home/davidanekstein/immunant/ia2/docs/conversation_repro_log_2026-03-27.md`
