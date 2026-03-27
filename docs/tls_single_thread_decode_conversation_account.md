# TLS + Single-Thread Decode Conversation Account

## Scope
This document summarizes the work completed in this chat thread on `fix/shared-tls-tcb-rationale-docs`, including investigation, attempted fixes, branch/worktree workflows, decode verification, and the latest TLS repro work.

## Initial Objective
- Reproduce the dav1d decode failure that surfaced as an `__tls_get_addr` / "rrltd tls get addr" style runtime fault under IA2 compartment switching.
- Determine root cause.
- Identify fixes aligned with IA2 philosophy (prefer conservative, memory-safe approaches; avoid over-broad pkey0 sharing).
- Document everything clearly and reproducibly.

## Chronological Summary

### Phase 1: Reproduction and Root-Cause Investigation
- Checked out and worked from `fix/shared-tls-tcb-rationale-docs`.
- Used recent markdown notes and prior runbooks to make dav1d execution reproducible.
- Reproduced failure modes tied to TLS access during compartment transitions.
- Investigated whether failure was due to local write targets (e.g., `res`) versus protection state visibility.
- Conclusion from investigation: the meaningful issue was PKRU/TLS visibility during transition windows, not a simple local-variable write bug.

### Phase 2: Architecture and Policy Questions
- Evaluated whether callgates and explicit compartment switching were legitimate. Conclusion: yes, legitimate and expected in IA2, but correctness depends on preserving ABI-sensitive state across transitions.
- Assessed whether glibc bump allocator compartmentalization was the primary cause. Conclusion: not the core root cause of the observed `__tls_get_addr`-style failure.
- Compared fix directions on a spectrum:
  - Very conservative/IA2-aligned.
  - Less conservative but expedient (including broad TLS sharing ideas).
- Repeatedly revisited whether specific carve-outs should be pkey0 and what that implies for policy/security.

### Phase 3: Multiple-Approach Workflow (Worktrees + Logs)
- Stashed state and used separate worktrees for multiple approaches from clean baselines.
- Maintained append-only logs while testing approaches.
- Explored options including broader sharing and narrower carve-outs.
- Evaluated regressions (including `--version` behavior), fixed artifacts when possible, and continued only when the approach remained technically coherent.

### Phase 4: Single-Thread Decode Progress
- Focus shifted to unblocking single-thread decode first.
- Confirmed a working single-thread decode path in at least one commit/worktree state.
- Tested commit-by-commit behavior to reduce stale-artifact risk and isolate effective deltas.
- Rebuilt from fresh artifacts repeatedly to validate claims.
- Clarified that prior passing/failing inconsistencies can come from stale build/layout state if build dirs are reused incorrectly.

### Phase 5: Documentation and PR Hygiene
- Wrote/expanded markdown reports and reproduction notes.
- Reworked commit messages to remove debugging-internal wording and replace with reviewer-usable problem/fix/why framing.
- Renamed branch to a shorter name reflecting the real fix intent.
- Pushed and repushed as needed after message updates and branch naming cleanup.

### Phase 6: Invariant Clarification (Critical)
- Clarified a recurring confusion:
  - The current one-page assumption is **not** "PT_TLS must be <= one page".
  - The practical assumption in current x86_64 policy is about which TLS pages are explicitly kept shared (notably `ia2_stackptr_0` page and thread-pointer/TCB page) while retagging PT_TLS ranges.
- Confirmed that total PT_TLS across loaded libraries can exceed one page without immediate failure.
- Discussed when one-page assumptions can fail in principle and how that differs from observed practical behavior.

### Phase 7: New Repro Work (Latest actions in this thread)
- Replaced prior forced-PKRU style repro path with a more natural cross-compartment TLS repro scaffold under:
  - `tests/tls_one_page_repro/`
- Added parameterized TLS sizes for main/lib2/shared test objects so layout pressure can be swept without rewriting source.
- Added runtime diagnostics to log:
  - distance from thread pointer
  - per-address pkey via `/proc/self/smaps`
- Added automated sweep script:
  - `tests/tls_one_page_repro/sweep_tls_layout.sh`
- Added append-only run log:
  - `tests/tls_one_page_repro/attempt_log.md`

## Latest Concrete Results (from the new repro)
- Baseline run (default sizing) passed.
- Bounded sweep executed 40 configurations (main/shared/lib2 TLS size matrix) and completed with:
  - `sweep-complete no-failure`
- Across sweep runs, diagnostics consistently showed:
  - `pkey(tp)=0`
  - `pkey(main_tls)=1`
  - `pkey(shared_tls)=0`
  - `pkey(lib2_tls)=2`
- Observed large offsets from TP (including >100KB in tested configs) without natural failure in this harness.

## Key Decisions and Explanations Settled in Chat
- Do not describe this as a PT_TLS-size invariant issue.
- Describe it as a carve-out sufficiency issue under PKRU transitions.
- Distinguish clearly:
  - ABI-level facts (e.g., `%fs`/TCB canary access behavior).
  - Runtime/layout assumptions (whether current carve-outs remain sufficient for all touched TLS state).

## Current Tree Status (at time of writing)
- Modified tracked file:
  - `tests/CMakeLists.txt` (adds `tls_one_page_repro` subdir)
- New untracked test directory with repro assets/logging:
  - `tests/tls_one_page_repro/`

## What Is Still Open
- A natural, deterministic failing case for the carve-out insufficiency has not yet been reproduced in the new harness.
- Multi-thread decode remains intentionally out of scope for the most recent phase; single-thread behavior and TLS-policy clarity were prioritized.
- If needed next, we can extend the repro to target additional realistic TLS-touch paths while keeping policy assumptions explicit.

## Repro Commands Used in Latest Phase
```bash
# Build the new repro target in the existing build
ninja -C build/x86_64 tls_one_page_repro

# Run baseline repro
build/x86_64/tests/tls_one_page_repro/tls_one_page_repro

# Run bounded size sweep (appends to attempt_log.md)
tests/tls_one_page_repro/sweep_tls_layout.sh
```

## Notes on Terminology
- "Unable to reproduce" in this context means: unable to trigger a natural failure in the new harness/sweep, not proof that no failing layout exists.
- "Potential risk in principle" remains valid unless we can prove sufficiency of carve-outs or enforce stronger invariants.
