# Compartment-Aware Exit Handling Plan

## Objectives
- Preserve compartment isolation while process destructors run by switching PKRU per owning compartment instead of dropping to zero.
- Leverage libmpk-style metadata protection and lazy PKRU updates for a production-feasible design.
- Provide incremental validation so regressions surface before the full `_dl_fini` integration.

## Status Snapshot (September 30, 2025)
- `dl_debug_exit_phase0` now exits with status 0 under `ctest`, removing the harness crash that previously blocked this roadmap.
- Treat the phase breakdown below as pending until the rewriter metadata export and exit-path logging patches land.

## Phase 0 – Baseline Capture (Week 1)
**Tasks**
- Instrument the rewriter to emit a JSON or ELF note sidecar mapping each destructor symbol to its compartment ID.
- Extend `tests/dl_debug_test/library.c` to register a synthetic destructor per compartment for verification.
- Add a debug-only logging hook in `runtime/libia2` to dump the active PKRU and target compartment before each destructor executes (behind `IA2_TRACE_EXIT=1`).

**Testing**
- New Criterion test `dl_debug.metadata_smoke` that asserts the emitted map contains expected destructor symbols.
- `ninja dl_debug_test` must pass with tracing enabled and disabled.
- Manual check: run `IA2_TRACE_EXIT=1 build/tests/dl_debug_test/dl_debug_test` and confirm log ordering matches map IDs.

## Phase 1 – Metadata Residency & Protection (Week 2)
**Tasks**
- Allocate the destructor map in a read-only shared page managed by `libia2`, mirroring libmpk’s kernel/user split (userspace read, runtime write only during init).
- Add integrity assertions so tampering triggers an early abort (checksum + bounds check).
- Provide a `ia2_destructor_lookup(const void *fn)` helper returning compartment metadata.

**Testing**
- Add unit test under `tests/unit/` that fuzzes `ia2_destructor_lookup` with valid and invalid pointers.
- Extend `ninja dl_debug_test` to run with ASAN (use existing `scripts/run_with_asan.sh`) ensuring metadata path stays read-only.

## Phase 2 – Compartment Switcher (Weeks 3–4)
**Tasks**
- Implement `ia2_run_destructor(const struct exit_record *)` that: saves current PKRU, loads the destructor’s compartment PKRU, invokes the function, restores the previous value.
- Hook `__cxa_atexit` and `atexit` wrappers to store `exit_record` metadata upon registration.
- Add a fall-back path for unknown compartment IDs: temporarily set PKRU=0 but emit a structured log for remediation.

**Testing**
- Create a focused test target `ninja dl_debug_exit_phase2` that registers mixed-compartment destructors and asserts via logging that PKRU transitions occur (parse logs in test harness).
- Update existing `dl_debug` Criterion suite to remove the inline `wrpkru` workaround and ensure all tests still pass.
- Introduce a GDB recipe in the doc to breakpoint `ia2_run_destructor` and inspect PKRU before/after.

## Phase 3 – `_dl_fini` Integration (Weeks 5–6)
**Tasks**
- Patch the glibc shim or LD_AUDIT hook to call `ia2_run_destructor` for each `_dl_fini` invocation, passing the owning `link_map` pointer so we translate to a compartment ID.
- Implement lazy synchronization akin to libmpk: queue PKRU swaps via a task list so only the executing thread updates its register right before the destructor call.
- Add metrics counters (success, fallback, unknown) exposed via `IA2_DIAG_EXIT=1` for post-mortem validation.

**Testing**
- Extend `tests/dl_debug_test` with a new scenario `compartment_fini` that dynamically loads/unloads a compartmented DSO and checks counters.
- Run `ninja check` on both the patched sysroot and the vanilla one to detect ABI mismatches early.
- Validate on MPK-capable hardware and a non-MPK fallback host (expect graceful downgrade with logged warning).

## Phase 4 – Hardening & Cleanup (Week 7)
**Tasks**
- Replace fallback PKRU=0 with compartment-to-compartment dependency annotations (future work stub if metadata absent).
- Document escape hatches (`IA2_EXIT_ALLOW_ALL=1`) and default to secure path in production builds.
- Update `docs/exit-handling.md` summarizing guarantees, knobs, and troubleshooting steps.

**Testing**
- Add regression test toggling `IA2_EXIT_ALLOW_ALL=1` to ensure bypass remains intentional.
- Re-run `./scripts/check_two_keys_ldd.sh --target dl_debug_test` and `./scripts/check_two_keys_symbols.sh --target dl_debug_test` to certify loader metadata remains consistent.

## Incremental Review Points
- End of each phase: open a short design-review PR with collected logs, updated tests, and `ninja check` output.
- Maintain a rolling checklist in `tests/dl_debug_test/README.md` noting which workaround (PKRU=0) remains and when it is safe to remove.

## Rollback & Monitoring
- Keep the existing `wrpkru` workaround guarded by `IA2_EXIT_FORCE_LEGACY`. If phased changes regress, flip the flag in CI to maintain coverage while investigating.
- Integrate counter telemetry into existing Criterion output so CI dashboards catch unexpected fallback usage.
