# Libc Compartment Regression

- **Failure snapshot:** CI log captured in `docs/test_failure.txt` (see `docs/test_failure.txt:192-722`). Multiple lit cases such as `IA2 :: minimal/main.c` and `IA2 :: global_fn_ptr/main.c` reported “Unexpected link args” because their `*_call_gates_0.ld` files were no longer empty. Others (e.g., `tests/minimal/minimal.c`, `tests/read_config/main.c`) failed `FileCheck` since expected `--wrap=` entries vanished.

- **Root cause:** The recent libc/ld.so compartment patch introduced unconditional bookkeeping in `tools/rewriter/SourceRewriter.cpp:1547+` that assigns every system-header function to compartment 1 and emits wrappers for them, even when the new `--libc-compartment` flag is not provided. Default IA2 targets (which never pass that flag) therefore started generating wrappers for compartment 0, breaking the invariant that `_call_gates_0.ld` stays empty.

- **Fix:** Added a guard around the system-header logic so it only runs when `gLibcCompartmentEnabled` (i.e., `--libc-compartment`) is true. This keeps the new behavior available for dl-debug scenarios that opt in via `EXTRA_REWRITER_ARGS --libc-compartment`, while restoring the legacy behavior for the rest of the suite.

- **Verification command:**
  ```
  ninja -C build tests/minimal/minimal
  ```
  After running the command, `build/tests/minimal/minimal_call_gates_0.ld` is empty again, confirming that the regression no longer affects the non-libc configuration.
