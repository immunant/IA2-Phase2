# TLS Unblock Approach Log

Append-only. Each entry includes exact command(s), expected outcome, and observed outcome.

## 2026-03-21T23:50:31Z
- Step: Initialized clean approach worktree.
- Commands:
  -         - Notes: Branch and baseline commit recorded via git worktree setup.


## 2026-03-21T23:57:53Z
- Intent: Configure IA2 for the x86_64 Debug build.
- Commands:
  - cmake -S /home/davidanekstein/immunant/approaches/ia2-a -B /home/davidanekstein/immunant/approaches/ia2-a/build/x86_64 -GNinja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=/usr/lib/llvm-18/bin/clang -DCMAKE_CXX_COMPILER=/usr/lib/llvm-18/bin/clang++
- Observed result: Succeeded. CMake generated Ninja files in /home/davidanekstein/immunant/approaches/ia2-a/build/x86_64. CMake emitted a warning that LLVM_EXTERNAL_LIT was set to /usr/lib/llvm-18/bin/llvm-lit, which does not exist, but configuration completed.

## 2026-03-21T23:58:24Z
- Intent: Build IA2 runtime targets needed for dav1d.
- Commands:
  - ninja -C /home/davidanekstein/immunant/approaches/ia2-a/build/x86_64 libia2 partition-alloc-padding
- Observed result: Succeeded. Built  and , and the dependency chain also rebuilt  and .

## 2026-03-21T23:58:39Z
- Intent: Configure dav1d with the Meson binary specified in the task.
- Commands:
  - /home/davidanekstein/immunant/.cache/uv/environments-v2/rewrite-11b56381038e01c7/bin/meson setup /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64 /home/davidanekstein/immunant/approaches/dav1d-a --buildtype=debug -Dia2_path=/home/davidanekstein/immunant/approaches/ia2-a -Dia2_build_path=/home/davidanekstein/immunant/approaches/ia2-a/build/x86_64 -Dia2_enable=true -Dia2_permissive_mode=false -Dia2_libc_compartment=true
- Observed result: Failed immediately with `/bin/bash: line 1: /home/davidanekstein/immunant/.cache/uv/environments-v2/rewrite-11b56381038e01c7/bin/meson: No such file or directory`.

## 2026-03-21T23:58:46Z
- Intent: Configure dav1d with Meson 1.10.1 and the IA2 libc compartment options.
- Commands:
  - /home/davidanekstein/.cache/uv/environments-v2/rewrite-11b56381038e01c7/bin/meson setup /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64 /home/davidanekstein/immunant/approaches/dav1d-a --buildtype=debug -Dia2_path=/home/davidanekstein/immunant/approaches/ia2-a -Dia2_build_path=/home/davidanekstein/immunant/approaches/ia2-a/build/x86_64 -Dia2_enable=true -Dia2_permissive_mode=false -Dia2_libc_compartment=true
- Observed result: Succeeded. Meson 1.10.1 generated the dav1d x86_64 native build in /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64 with IA2 enabled and ia2_libc_compartment=true.

## 2026-03-22T00:03:43Z
- Intent: Restore the generated IA2 callgate artifacts into the dav1d worktree.
- Commands:
  - cp /home/davidanekstein/immunant/approaches/dav1d-b/callgate_wrapper.h /home/davidanekstein/immunant/approaches/dav1d-a/callgate_wrapper.h
  - cp /home/davidanekstein/immunant/approaches/dav1d-b/callgate_wrapper.c /home/davidanekstein/immunant/approaches/dav1d-a/callgate_wrapper.c
  - cp /home/davidanekstein/immunant/approaches/dav1d-b/callgate_wrapper_0.ld /home/davidanekstein/immunant/approaches/dav1d-a/callgate_wrapper_0.ld
  - cp /home/davidanekstein/immunant/approaches/dav1d-b/callgate_wrapper_1.ld /home/davidanekstein/immunant/approaches/dav1d-a/callgate_wrapper_1.ld
  - cp /home/davidanekstein/immunant/approaches/dav1d-b/callgate_wrapper_2.ld /home/davidanekstein/immunant/approaches/dav1d-a/callgate_wrapper_2.ld
- Observed result: Succeeded. `dav1d-a` now has the generated callgate shim files needed by the IA2 Meson link flags.

## 2026-03-22T00:03:43Z
- Intent: Build the `libcallgates.so` shim used by the dav1d IA2 build.
- Commands:
  - clang -shared -fPIC -Wl,-z,now /home/davidanekstein/immunant/approaches/dav1d-a/callgate_wrapper.c -I /home/davidanekstein/immunant/approaches/ia2-a/runtime/libia2/include -o /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/src/libcallgates.so
- Observed result: Succeeded. Produced `build/x86_64/src/libcallgates.so`.

## 2026-03-22T00:03:43Z
- Intent: Provide the custom IA2 loader and allocator runtime artifacts expected by the dav1d executable.
- Commands:
  - cp /lib64/ld-linux-x86-64.so.2 /home/davidanekstein/immunant/approaches/ia2-a/build/x86_64/runtime/libia2/ld-linux-x86-64.so.2
  - cp /home/davidanekstein/immunant/approaches/ia2-a/build/x86_64/runtime/partition-alloc/libpartition-alloc.so /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/src/libpartition-alloc.so
- Observed result: Succeeded. The dav1d interpreter path and `libpartition-alloc.so` dependency are now present in the expected runtime locations.

## 2026-03-22T00:03:43Z
- Intent: Rebuild dav1d after restoring the generated callgate/runtime artifacts.
- Commands:
  - ninja -C /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64 tools/dav1d src/libdav1d.so.7.0.0
- Observed result: Succeeded. Ninja linked `src/libdav1d.so.7.0.0` and `tools/dav1d` successfully.

## 2026-03-22T00:03:43Z
- Intent: Verify runtime behavior of `dav1d --version` and capture the remaining crash if present.
- Commands:
  - gdb -q --batch -ex 'set pagination off' -ex run -ex 'info registers rip rsp rbp rax rbx rcx rdx rsi rdi pkru' -ex bt --args /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/tools/dav1d --version
- Observed result: Still crashes. GDB reported SIGSEGV in `__wrap_main` with `pkru=0xfffffff0`; backtrace was `__wrap_main -> __libc_start_call_main -> __libc_start_main_impl -> _start`.

## 2026-03-22T00:03:43Z
- Intent: Check whether the requested IVF repro file exists before running the decode test.
- Commands:
  - test -f /tmp/test.ivf && /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/tools/dav1d -i /tmp/test.ivf -o /dev/null || echo 'missing /tmp/test.ivf'
- Observed result: `/tmp/test.ivf` was missing, so the decode repro could not be run as written.
## 2026-03-22T00:12:53Z
- Intent: Run a uniform smoke/decode check with a stable local IVF sample.
- Commands:
  - LD_LIBRARY_PATH=/home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/src:/home/davidanekstein/immunant/approaches/ia2-a/build/x86_64/runtime/libia2:/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/tools/dav1d --version
  - LD_LIBRARY_PATH=/home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/src:/home/davidanekstein/immunant/approaches/ia2-a/build/x86_64/runtime/libia2:/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/tools/dav1d -i /home/davidanekstein/immunant/test.ivf -o /dev/null
- Observed result: both commands exited with SIGSEGV (exit 139).

## 2026-03-22T00:15:58Z
- Intent: Apply known prerequisite baseline fix shared across approaches (x86_64 TCB+stackptr TLS carve-out and partition-alloc initial-exec TLS model) to remove confounding startup regressions.
- Commands:
  - git -C /home/davidanekstein/immunant/ia2 show stash@{0}:runtime/libia2/ia2.c > /home/davidanekstein/immunant/approaches/ia2-a/runtime/libia2/ia2.c
  - git -C /home/davidanekstein/immunant/ia2 show stash@{0}:runtime/partition-alloc/CMakeLists.txt > /home/davidanekstein/immunant/approaches/ia2-a/runtime/partition-alloc/CMakeLists.txt
  - ninja -C /home/davidanekstein/immunant/approaches/ia2-a/build/x86_64 libia2 partition-alloc-padding
  - ninja -C /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64 tools/dav1d src/libdav1d.so.7.0.0
- Observed result: rebuild completed for approach a.

## 2026-03-22T00:15:58Z
- Intent: Re-test approach a after prerequisite baseline fix.
- Commands:
  - LD_LIBRARY_PATH=/home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/src:/home/davidanekstein/immunant/approaches/ia2-a/build/x86_64/runtime/libia2:/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/tools/dav1d --version
  - gdb -q --batch -ex 'set pagination off' -ex 'set env LD_LIBRARY_PATH=/home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/src:/home/davidanekstein/immunant/approaches/ia2-a/build/x86_64/runtime/libia2:/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu' -ex 'run --version' -ex 'bt 12' -ex 'info registers rip r11 fs_base pkru' --args /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/tools/dav1d
  - gdb -q --batch -ex 'set pagination off' -ex 'set env LD_LIBRARY_PATH=/home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/src:/home/davidanekstein/immunant/approaches/ia2-a/build/x86_64/runtime/libia2:/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu' -ex 'run -i /home/davidanekstein/immunant/test.ivf -o /dev/null' -ex 'bt 12' -ex 'info registers rip rax fs_base pkru' --args /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/tools/dav1d
- Observed result:  prints and then segfaults during  with ; decode now runs further but segfaults in  with .

## 2026-03-22T00:17:30Z
- Intent: Corrected recording of the prior re-test outcome for approach a.
- Commands:
  - LD_LIBRARY_PATH=/home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/src:/home/davidanekstein/immunant/approaches/ia2-a/build/x86_64/runtime/libia2:/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/tools/dav1d --version
  - gdb -q --batch -ex 'set pagination off' -ex 'set env LD_LIBRARY_PATH=/home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/src:/home/davidanekstein/immunant/approaches/ia2-a/build/x86_64/runtime/libia2:/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu' -ex 'run --version' -ex 'bt 12' -ex 'info registers rip r11 fs_base pkru' --args /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/tools/dav1d
  - gdb -q --batch -ex 'set pagination off' -ex 'set env LD_LIBRARY_PATH=/home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/src:/home/davidanekstein/immunant/approaches/ia2-a/build/x86_64/runtime/libia2:/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu' -ex 'run -i /home/davidanekstein/immunant/test.ivf -o /dev/null' -ex 'bt 12' -ex 'info registers rip rax fs_base pkru' --args /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/tools/dav1d
- Observed result:
  - `--version` prints `1.4.2-187-g39c386a`, then segfaults in `ia2_compartment_destructor_1` with `pkru=0xfffffffc`.
  - Decode advances to input read path, then segfaults in `__memcpy_evex_unaligned_erms` with `pkru=0xfffffff0`.

## 2026-03-22T00:29:47Z
- Intent: Controlled regression isolation for  crash: revert only  to branch HEAD in approach A and rebuild.
- Commands:
  - git -C /home/davidanekstein/immunant/approaches/ia2-a checkout -- runtime/libia2/ia2.c
  - ninja -C /home/davidanekstein/immunant/approaches/ia2-a/build/x86_64 libia2
  - ninja -C /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64 tools/dav1d
  - /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/tools/dav1d --version

## 2026-03-22T00:30:38Z
- Intent: Record controlled isolation results for A `--version` regression.
- Commands:
  - Reverted only `runtime/libia2/ia2.c` to branch HEAD, rebuilt `libia2` and `tools/dav1d`, then ran `tools/dav1d --version`.
  - Restored `runtime/libia2/ia2.c` from `stash@{0}`, rebuilt, and re-ran `tools/dav1d --version`.
- Observed result:
  - With reverted `ia2.c`: early SIGSEGV in `__wrap_main` with `pkru=0xfffffff0` before version print.
  - With restored `ia2.c` baseline patch: version prints, then SIGSEGV on shutdown (exit 139).
  - Conclusion: not a random artifact; A has two distinct crash surfaces depending on TLS-protection behavior.

## 2026-03-22T00:59:20Z
- Intent: Resolve `--version` regression artifact by checking config parity between IA2 and dav1d in approach A.
- Commands:
  - `rg -n 'IA2_LIBC_COMPARTMENT' /home/davidanekstein/immunant/approaches/ia2-a/build/x86_64/CMakeCache.txt`
  - `rg -n 'ia2_libc_compartment' /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/meson-private/cmd_line.txt`
  - `git -C /home/davidanekstein/immunant/approaches/ia2-a submodule update --init --recursive external/glibc external/b63` (and pinned missing glibc commit by fetching/checking out `6e352929...`)
  - `cmake -S /home/davidanekstein/immunant/approaches/ia2-a -B /home/davidanekstein/immunant/approaches/ia2-a/build/x86_64 -DIA2_LIBC_COMPARTMENT=ON`
  - `ninja -C /home/davidanekstein/immunant/approaches/ia2-a/build/x86_64 libia2 partition-alloc-padding`
  - `ninja -C /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64 tools/dav1d`
  - `env LD_LIBRARY_PATH=/home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/src:/home/davidanekstein/immunant/approaches/ia2-a/build/x86_64/runtime/libia2:/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/tools/dav1d --version`
  - `env LD_LIBRARY_PATH=/home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/src:/home/davidanekstein/immunant/approaches/ia2-a/build/x86_64/runtime/libia2:/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/tools/dav1d -i /home/davidanekstein/immunant/test.ivf -o /dev/null`
- Observed result:
  - Pre-fix mismatch confirmed: IA2 was built with `IA2_LIBC_COMPARTMENT=OFF` while dav1d was built with `ia2_libc_compartment=true`.
  - After aligning A to `IA2_LIBC_COMPARTMENT=ON`, `--version` succeeded (`exit 0`).
  - Decode still crashes (`exit 139`), so the prior `--version` failure in A was an environment/config artifact, not the decode root cause.

## 2026-03-22T02:24:22Z
- Intent: Resume Option A with strict append-only logging and continue from the known decode SIGSEGV site.
- Commands:
  -  M runtime/libia2/ia2.c
 M runtime/partition-alloc/CMakeLists.txt
?? docs/tls-unblock-approach-log.md
?? docs/tls-unblock-three-approaches.md
  -  M tools/dav1d.c
?? callgate_wrapper.c
?? callgate_wrapper.h
?? callgate_wrapper_0.ld
?? callgate_wrapper_1.ld
?? callgate_wrapper_2.ld
  - - Observed result: Succeeded. Ninja linked `src/libdav1d.so.7.0.0` and `tools/dav1d` successfully.

## 2026-03-22T00:03:43Z
- Intent: Verify runtime behavior of `dav1d --version` and capture the remaining crash if present.
- Commands:
  - gdb -q --batch -ex 'set pagination off' -ex run -ex 'info registers rip rsp rbp rax rbx rcx rdx rsi rdi pkru' -ex bt --args /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/tools/dav1d --version
- Observed result: Still crashes. GDB reported SIGSEGV in `__wrap_main` with `pkru=0xfffffff0`; backtrace was `__wrap_main -> __libc_start_call_main -> __libc_start_main_impl -> _start`.

## 2026-03-22T00:03:43Z
- Intent: Check whether the requested IVF repro file exists before running the decode test.
- Commands:
  - test -f /tmp/test.ivf && /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/tools/dav1d -i /tmp/test.ivf -o /dev/null || echo 'missing /tmp/test.ivf'
- Observed result: `/tmp/test.ivf` was missing, so the decode repro could not be run as written.
## 2026-03-22T00:12:53Z
- Intent: Run a uniform smoke/decode check with a stable local IVF sample.
- Commands:
  - LD_LIBRARY_PATH=/home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/src:/home/davidanekstein/immunant/approaches/ia2-a/build/x86_64/runtime/libia2:/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/tools/dav1d --version
  - LD_LIBRARY_PATH=/home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/src:/home/davidanekstein/immunant/approaches/ia2-a/build/x86_64/runtime/libia2:/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/tools/dav1d -i /home/davidanekstein/immunant/test.ivf -o /dev/null
- Observed result: both commands exited with SIGSEGV (exit 139).

## 2026-03-22T00:15:58Z
- Intent: Apply known prerequisite baseline fix shared across approaches (x86_64 TCB+stackptr TLS carve-out and partition-alloc initial-exec TLS model) to remove confounding startup regressions.
- Commands:
  - git -C /home/davidanekstein/immunant/ia2 show stash@{0}:runtime/libia2/ia2.c > /home/davidanekstein/immunant/approaches/ia2-a/runtime/libia2/ia2.c
  - git -C /home/davidanekstein/immunant/ia2 show stash@{0}:runtime/partition-alloc/CMakeLists.txt > /home/davidanekstein/immunant/approaches/ia2-a/runtime/partition-alloc/CMakeLists.txt
  - ninja -C /home/davidanekstein/immunant/approaches/ia2-a/build/x86_64 libia2 partition-alloc-padding
  - ninja -C /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64 tools/dav1d src/libdav1d.so.7.0.0
- Observed result: rebuild completed for approach a.

## 2026-03-22T00:15:58Z
- Intent: Re-test approach a after prerequisite baseline fix.
- Commands:
  - LD_LIBRARY_PATH=/home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/src:/home/davidanekstein/immunant/approaches/ia2-a/build/x86_64/runtime/libia2:/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/tools/dav1d --version
  - gdb -q --batch -ex 'set pagination off' -ex 'set env LD_LIBRARY_PATH=/home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/src:/home/davidanekstein/immunant/approaches/ia2-a/build/x86_64/runtime/libia2:/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu' -ex 'run --version' -ex 'bt 12' -ex 'info registers rip r11 fs_base pkru' --args /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/tools/dav1d
  - gdb -q --batch -ex 'set pagination off' -ex 'set env LD_LIBRARY_PATH=/home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/src:/home/davidanekstein/immunant/approaches/ia2-a/build/x86_64/runtime/libia2:/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu' -ex 'run -i /home/davidanekstein/immunant/test.ivf -o /dev/null' -ex 'bt 12' -ex 'info registers rip rax fs_base pkru' --args /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/tools/dav1d
- Observed result:  prints and then segfaults during  with ; decode now runs further but segfaults in  with .

## 2026-03-22T00:17:30Z
- Intent: Corrected recording of the prior re-test outcome for approach a.
- Commands:
  - LD_LIBRARY_PATH=/home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/src:/home/davidanekstein/immunant/approaches/ia2-a/build/x86_64/runtime/libia2:/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/tools/dav1d --version
  - gdb -q --batch -ex 'set pagination off' -ex 'set env LD_LIBRARY_PATH=/home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/src:/home/davidanekstein/immunant/approaches/ia2-a/build/x86_64/runtime/libia2:/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu' -ex 'run --version' -ex 'bt 12' -ex 'info registers rip r11 fs_base pkru' --args /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/tools/dav1d
  - gdb -q --batch -ex 'set pagination off' -ex 'set env LD_LIBRARY_PATH=/home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/src:/home/davidanekstein/immunant/approaches/ia2-a/build/x86_64/runtime/libia2:/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu' -ex 'run -i /home/davidanekstein/immunant/test.ivf -o /dev/null' -ex 'bt 12' -ex 'info registers rip rax fs_base pkru' --args /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/tools/dav1d
- Observed result:
  - `--version` prints `1.4.2-187-g39c386a`, then segfaults in `ia2_compartment_destructor_1` with `pkru=0xfffffffc`.
  - Decode advances to input read path, then segfaults in `__memcpy_evex_unaligned_erms` with `pkru=0xfffffff0`.

## 2026-03-22T00:29:47Z
- Intent: Controlled regression isolation for  crash: revert only  to branch HEAD in approach A and rebuild.
- Commands:
  - git -C /home/davidanekstein/immunant/approaches/ia2-a checkout -- runtime/libia2/ia2.c
  - ninja -C /home/davidanekstein/immunant/approaches/ia2-a/build/x86_64 libia2
  - ninja -C /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64 tools/dav1d
  - /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/tools/dav1d --version

## 2026-03-22T00:30:38Z
- Intent: Record controlled isolation results for A `--version` regression.
- Commands:
  - Reverted only `runtime/libia2/ia2.c` to branch HEAD, rebuilt `libia2` and `tools/dav1d`, then ran `tools/dav1d --version`.
  - Restored `runtime/libia2/ia2.c` from `stash@{0}`, rebuilt, and re-ran `tools/dav1d --version`.
- Observed result:
  - With reverted `ia2.c`: early SIGSEGV in `__wrap_main` with `pkru=0xfffffff0` before version print.
  - With restored `ia2.c` baseline patch: version prints, then SIGSEGV on shutdown (exit 139).
  - Conclusion: not a random artifact; A has two distinct crash surfaces depending on TLS-protection behavior.

## 2026-03-22T00:59:20Z
- Intent: Resolve `--version` regression artifact by checking config parity between IA2 and dav1d in approach A.
- Commands:
  - `rg -n 'IA2_LIBC_COMPARTMENT' /home/davidanekstein/immunant/approaches/ia2-a/build/x86_64/CMakeCache.txt`
  - `rg -n 'ia2_libc_compartment' /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/meson-private/cmd_line.txt`
  - `git -C /home/davidanekstein/immunant/approaches/ia2-a submodule update --init --recursive external/glibc external/b63` (and pinned missing glibc commit by fetching/checking out `6e352929...`)
  - `cmake -S /home/davidanekstein/immunant/approaches/ia2-a -B /home/davidanekstein/immunant/approaches/ia2-a/build/x86_64 -DIA2_LIBC_COMPARTMENT=ON`
  - `ninja -C /home/davidanekstein/immunant/approaches/ia2-a/build/x86_64 libia2 partition-alloc-padding`
  - `ninja -C /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64 tools/dav1d`
  - `env LD_LIBRARY_PATH=/home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/src:/home/davidanekstein/immunant/approaches/ia2-a/build/x86_64/runtime/libia2:/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/tools/dav1d --version`
  - `env LD_LIBRARY_PATH=/home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/src:/home/davidanekstein/immunant/approaches/ia2-a/build/x86_64/runtime/libia2:/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu /home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/tools/dav1d -i /home/davidanekstein/immunant/test.ivf -o /dev/null`
- Observed result:
  - Pre-fix mismatch confirmed: IA2 was built with `IA2_LIBC_COMPARTMENT=OFF` while dav1d was built with `ia2_libc_compartment=true`.
  - After aligning A to `IA2_LIBC_COMPARTMENT=ON`, `--version` succeeded (`exit 0`).
  - Decode still crashes (`exit 139`), so the prior `--version` failure in A was an environment/config artifact, not the decode root cause.
- Observed result: Resumed with prior state intact ( fixed; decode still SIGSEGV), and continuing targeted root-cause analysis.

## 2026-03-22T02:24:33Z
- Intent: Correct previous malformed log append and continue Option A decode root-cause mapping.
- Commands:
  - `tail -n 40 /home/davidanekstein/immunant/approaches/ia2-a/docs/tls-unblock-approach-log.md`
  - `cat >> /home/davidanekstein/immunant/approaches/ia2-a/docs/tls-unblock-approach-log.md <<'EOF' ... EOF`
- Observed result: Previous append had shell interpolation artifacts from unquoted heredoc; this corrective entry preserves append-only history and restores clean formatting.

## 2026-03-22T02:34:01Z
- Intent: Validate whether Option A decode crash is caused by stale copied `libpartition-alloc.so` in dav1d worktree.
- Commands:
  - `sha256sum <dav1d-a>/build/x86_64/src/libpartition-alloc.so <ia2-a>/build/x86_64/runtime/partition-alloc/libpartition-alloc.so`
  - `cp -f <ia2-a>/build/x86_64/runtime/partition-alloc/libpartition-alloc.so <dav1d-a>/build/x86_64/src/libpartition-alloc.so`
  - rerun `tools/dav1d --version` and decode repro with standard `LD_LIBRARY_PATH`.
- Observed result:
  - Pre-sync hashes differed; post-sync hashes matched.
  - `--version` stayed successful (`exit 0`).
  - Decode still segfaulted (`exit 139`), so stale copy was a real artifact but not the remaining root cause.

## 2026-03-22T02:34:13Z
- Intent: Capture exact PKU fault page and prior pkey_mprotect operations for Option A decode.
- Commands:
  - `strace -f env LD_LIBRARY_PATH=<dav1d-a>/build/x86_64/src:<ia2-a>/build/x86_64/runtime/libia2:/usr/lib/x86_64-linux-gnu:/lib/x86_64-linux-gnu <dav1d-a>/build/x86_64/tools/dav1d -i /home/davidanekstein/immunant/test.ivf -o /dev/null`
  - `nl -ba /tmp/ia2_a_decode_full.strace | sed -n '140,250p'`
  - `tail -n 80 /tmp/ia2_a_decode_full.strace`
- Observed result:
  - Crash was `SEGV_PKUERR` at `si_addr=0x...6438fe8`, `si_pkey=1`.
  - Fault address is inside mapping named `ia2-loader-heap` (`mmap 0x...6437000, 32768`) that was tagged with `pkey=1`.
  - IA2 later retagged only pages `0x...643a000`, `0x...643b000`, and `0x...643c000` to `pkey=0`; the fault page `0x...6438000` remained `pkey=1`.
  - This explains why partition alloc TLS guard access at `libpartition-alloc.so+0x7d34b` still faults under cross compartment execution.

## 2026-03-22T02:43:25Z
- Intent: Option A code change to cover TLS pages below TCB during process TLS retag.
- Commands:
  - Edited runtime/libia2/ia2.c:
    - added helper add_shared_tls_page()
    - changed x86 TLS carveout policy to include pages from start_round_down up to tcb_page
  - Rebuilt IA2 and dav1d, synced libpartition-alloc.so into dav1d build src.
  - Re-tested version and decode.
- Observed result:
  - --version stayed exit 0.
  - Decode still failed at the same partition-alloc TLS guard instruction.
  - Strace proved the fault page stayed outside the retagged PT_TLS subrange, so the issue is not limited to the current module PT_TLS window.

## 2026-03-22T02:43:25Z
- Intent: Generalize Option A to retag loader static-TLS neighborhood around thread pointer to shared pkey 0.
- Commands:
  - Edited runtime/libia2/ia2.c: added ia2_unprotect_thread_pointer_mapping() using /proc/self/maps and TCB-neighborhood retagging.
  - Edited runtime/libia2/include/ia2_internal.h: added prototype for ia2_unprotect_thread_pointer_mapping().
  - Edited runtime/libia2/init.c: invoked ia2_unprotect_thread_pointer_mapping() in ia2_start().
  - Rebuilt and re-tested.
- Observed result:
  - Baseline decode command now succeeds end-to-end (exit 0).
  - --threads 2 still segfaults (exit 139).

## 2026-03-22T02:43:25Z
- Intent: Extend Option A thread handling to reduce inherited PKRU/thread-start TLS faults.
- Commands:
  - Edited runtime/libia2/threads.c:
    - call ia2_unprotect_thread_pointer_mapping() after init_stacks_and_setup_tls() in ia2_thread_begin()
    - temporarily set PKRU to PKRU(0) around __real_pthread_create and restore after call
  - Rebuilt IA2 and dav1d, re-ran single-thread and --threads 2 decode.
- Observed result:
  - Single-thread decode remains stable (exit 0).
  - --threads 2 still segfaults (exit 139), indicating remaining multi-thread TLS/PKRU transitions beyond current Option A fixes.

## 2026-03-22T02:43:38Z
- Intent: Keep consolidated three-approach summary document in sync with latest Option A results.
- Commands:
  - Edited docs/tls-unblock-three-approaches.md Current Status section.
- Observed result:
  - Document now records that single-thread decode succeeds and --threads 2 still crashes.

## 2026-03-22T02:59:56Z
- Intent: Produce a comprehensive markdown report covering all encountered problems, resolutions, and exact video decode reproduction steps.
- Commands:
  - Gathered/validated evidence from approach logs and current Option A state.
  - Wrote a new comprehensive report markdown in docs.
- Observed result: Report generation started.

## 2026-03-22T03:03:03Z
- Intent: Finalize comprehensive markdown report with full problem/resolution timeline and decode instructions.
- Commands:
  - Created "/home/davidanekstein/immunant/approaches/ia2-a/docs/tls-unblock-comprehensive-report.md".
  - Validated runtime status with:
    - "/home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/tools/dav1d --version"
    - "/home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/tools/dav1d -i /home/davidanekstein/immunant/test.ivf -o /dev/null"
    - "/home/davidanekstein/immunant/approaches/dav1d-a/build/x86_64/tools/dav1d --threads 2 -i /home/davidanekstein/immunant/test.ivf -o /dev/null"
- Observed result:
  - \ exit 0.
  - Single-thread decode exit 0.
  - \ still crashes (exit 139 without diag mode).

## 2026-03-22T03:03:54Z
- Intent: Correct the immediately previous malformed entry where unquoted heredoc stripped inline command markers.
- Commands:
  - Appended this correction without editing prior log text to preserve append-only history.
- Observed result:
  - Corrected interpretation of prior validation remains:
  - dav1d --version exited 0.
  - single-thread decode exited 0.
  - dav1d --threads 2 decode still crashed (exit 139 in non-diagnostic mode).

## 2026-03-22T03:10:20Z
- Intent: Expand comprehensive report with first-principles root-cause explanation and detailed fix rationale.
- Commands:
  - Edited `/home/davidanekstein/immunant/approaches/ia2-a/docs/tls-unblock-comprehensive-report.md`.
- Observed result:
  - Added an illustrative TLS/PKRU failure model (why `__tls_get_addr` symptom appears, why `res` is not the root cause).
  - Added step-by-step Option A fix explanation (what changed, where changed, why each step helps, and why single-thread passes while multi-thread remains open).

## 2026-03-22T18:05:14Z
- Intent: Determine whether partition-alloc  is necessary for successful dav1d invocation in current Option A state.
- Commands:
  - Temporarily switched  from  to .
  - Rebuilt IA2 (, ), synced  into dav1d build tree, rebuilt .
  - Ran:
    - 
    - 
  - Restored original CMakeLists file (), rebuilt, re-ran version/decode checks.
- Observed result:
  - With :  exited 0; single-thread decode exited 0.
  - After restore to :  exited 0; single-thread decode exited 0.
  - Multi-thread decode () still crashes in current Option A regardless of this specific check.
  - One shell-side artifact occurred while restoring with  set; file restoration was completed successfully using SHELL=/usr/bin/zsh
LSCOLORS=Gxfxcxdxbxegedabagacad
SESSION_MANAGER=local/Lemur:@/tmp/.ICE-unix/2358,unix/Lemur:/tmp/.ICE-unix/2358
WINDOWID=50331652
QT_ACCESSIBILITY=1
GHOSTTY_BIN_DIR=/snap/ghostty/718/bin
__EGL_VENDOR_LIBRARY_DIRS=/etc/glvnd/egl_vendor.d:/usr/share/glvnd/egl_vendor.d
COLORTERM=truecolor
XDG_CONFIG_DIRS=/etc/xdg/xdg-ubuntu-xorg:/etc/xdg
LESS=-R
XDG_MENU_PREFIX=gnome-
TERM_PROGRAM_VERSION=3.4
GNOME_DESKTOP_SESSION_ID=this-is-deprecated
TMUX=/tmp/tmux-1000/default,6444,0
GNOME_SHELL_SESSION_MODE=ubuntu
SSH_AUTH_SOCK=/run/user/1000/keyring/ssh
XDG_DATA_HOME=/home/davidanekstein/.local/share
XDG_CONFIG_HOME=/home/davidanekstein/.config
MEMORY_PRESSURE_WRITE=c29tZSAyMDAwMDAgMjAwMDAwMAA=
XMODIFIERS=@im=ibus
DESKTOP_SESSION=ubuntu-xorg
GDK_PIXBUF_MODULE_FILE=/home/davidanekstein/snap/ghostty/common/.cache/gdk-pixbuf-loaders.cache
GH_PAGER=cat
GTK_MODULES=gail:atk-bridge
PWD=/home/davidanekstein/immunant/ia2
LOGNAME=davidanekstein
XDG_SESSION_DESKTOP=ubuntu-xorg
XDG_SESSION_TYPE=x11
CODEX_MANAGED_BY_NPM=1
GPG_AGENT_INFO=/run/user/1000/gnupg/S.gpg-agent:0:1
SYSTEMD_EXEC_PID=2402
XAUTHORITY=/run/user/1000/gdm/Xauthority
GJS_DEBUG_TOPICS=JS ERROR;JS LOG
WINDOWPATH=2
GHOSTTY_SHELL_FEATURES=cursor:blink,path,title
HOME=/home/davidanekstein
USERNAME=davidanekstein
LANG=en_US.UTF-8
LS_COLORS=rs=0:di=01;34:ln=01;36:mh=00:pi=40;33:so=01;35:do=01;35:bd=40;33;01:cd=40;33;01:or=40;31;01:mi=00:su=37;41:sg=30;43:ca=00:tw=30;42:ow=34;42:st=37;44:ex=01;32:*.tar=01;31:*.tgz=01;31:*.arc=01;31:*.arj=01;31:*.taz=01;31:*.lha=01;31:*.lz4=01;31:*.lzh=01;31:*.lzma=01;31:*.tlz=01;31:*.txz=01;31:*.tzo=01;31:*.t7z=01;31:*.zip=01;31:*.z=01;31:*.dz=01;31:*.gz=01;31:*.lrz=01;31:*.lz=01;31:*.lzo=01;31:*.xz=01;31:*.zst=01;31:*.tzst=01;31:*.bz2=01;31:*.bz=01;31:*.tbz=01;31:*.tbz2=01;31:*.tz=01;31:*.deb=01;31:*.rpm=01;31:*.jar=01;31:*.war=01;31:*.ear=01;31:*.sar=01;31:*.rar=01;31:*.alz=01;31:*.ace=01;31:*.zoo=01;31:*.cpio=01;31:*.7z=01;31:*.rz=01;31:*.cab=01;31:*.wim=01;31:*.swm=01;31:*.dwm=01;31:*.esd=01;31:*.avif=01;35:*.jpg=01;35:*.jpeg=01;35:*.mjpg=01;35:*.mjpeg=01;35:*.gif=01;35:*.bmp=01;35:*.pbm=01;35:*.pgm=01;35:*.ppm=01;35:*.tga=01;35:*.xbm=01;35:*.xpm=01;35:*.tif=01;35:*.tiff=01;35:*.png=01;35:*.svg=01;35:*.svgz=01;35:*.mng=01;35:*.pcx=01;35:*.mov=01;35:*.mpg=01;35:*.mpeg=01;35:*.m2v=01;35:*.mkv=01;35:*.webm=01;35:*.webp=01;35:*.ogm=01;35:*.mp4=01;35:*.m4v=01;35:*.mp4v=01;35:*.vob=01;35:*.qt=01;35:*.nuv=01;35:*.wmv=01;35:*.asf=01;35:*.rm=01;35:*.rmvb=01;35:*.flc=01;35:*.avi=01;35:*.fli=01;35:*.flv=01;35:*.gl=01;35:*.dl=01;35:*.xcf=01;35:*.xwd=01;35:*.yuv=01;35:*.cgm=01;35:*.emf=01;35:*.ogv=01;35:*.ogx=01;35:*.aac=00;36:*.au=00;36:*.flac=00;36:*.m4a=00;36:*.mid=00;36:*.midi=00;36:*.mka=00;36:*.mp3=00;36:*.mpc=00;36:*.ogg=00;36:*.ra=00;36:*.wav=00;36:*.oga=00;36:*.opus=00;36:*.spx=00;36:*.xspf=00;36:*~=00;90:*#=00;90:*.bak=00;90:*.crdownload=00;90:*.dpkg-dist=00;90:*.dpkg-new=00;90:*.dpkg-old=00;90:*.dpkg-tmp=00;90:*.old=00;90:*.orig=00;90:*.part=00;90:*.rej=00;90:*.rpmnew=00;90:*.rpmorig=00;90:*.rpmsave=00;90:*.swp=00;90:*.tmp=00;90:*.ucf-dist=00;90:*.ucf-new=00;90:*.ucf-old=00;90:
XDG_CURRENT_DESKTOP=ubuntu:GNOME
MEMORY_PRESSURE_WATCH=/sys/fs/cgroup/user.slice/user-1000.slice/user@1000.service/session.slice/org.gnome.Shell@x11.service/memory.pressure
MANAGERPID=1998
XDG_CACHE_HOME=/home/davidanekstein/.cache
GJS_DEBUG_OUTPUT=stderr
GHOSTTY_RESOURCES_DIR=/snap/ghostty/current/share/ghostty
LESSCLOSE=/usr/bin/lesspipe %s %s
XDG_SESSION_CLASS=user
TERM=tmux-256color
TERMINFO=/snap/ghostty/current/share/terminfo
ZSH=/home/davidanekstein/.oh-my-zsh
CODEX_THREAD_ID=019d10d4-a709-7c53-8278-90aa14fddc4c
LESSOPEN=| /usr/bin/lesspipe %s
USER=davidanekstein
GIT_PAGER=cat
TMUX_PANE=%0
CLUTTER_DISABLE_MIPMAPPED_TEXT=1
DISPLAY=:0
SHLVL=2
GSM_SKIP_SSH_AGENT_WORKAROUND=true
PAGER=less
QT_IM_MODULE=ibus
NO_COLOR=1
CODEX_CI=1
LC_CTYPE=C.UTF-8
XDG_RUNTIME_DIR=/run/user/1000
DEBUGINFOD_URLS=https://debuginfod.ubuntu.com 
LC_ALL=C.UTF-8
XDG_DATA_DIRS=/usr/share/ubuntu-xorg:/usr/share/gnome:/usr/local/share/:/usr/share/:/var/lib/snapd/desktop
PATH=/home/davidanekstein/.local/bin:/home/davidanekstein/.codex/tmp/arg0/codex-arg07JHhSP:/home/davidanekstein/.npm-global/lib/node_modules/@openai/codex/node_modules/@openai/codex-linux-x64/vendor/x86_64-unknown-linux-musl/path:/home/davidanekstein/.npm-global/bin:/home/davidanekstein/.local/bin:/home/davidanekstein/.cargo/bin:/home/davidanekstein/.local/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/snap/bin:/snap/bin:/snap/ghostty/718/bin
GDMSESSION=ubuntu-xorg
DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus
OLDPWD=/home/davidanekstein
TERM_PROGRAM=tmux
_=/usr/bin/env.

## 2026-03-22T03:18:40Z
- Intent: Correct malformed prior append and preserve the actual A/B outcome for partition-alloc TLS model testing.
- Commands:
  - Temporarily changed partition-alloc TLS model from initial-exec to global-dynamic in approaches/ia2-a/runtime/partition-alloc/CMakeLists.txt.
  - Rebuilt IA2 runtime targets, synced libpartition-alloc.so into dav1d-a build src, rebuilt tools/dav1d.
  - Ran dav1d --version and single-thread decode against /home/davidanekstein/immunant/test.ivf.
  - Restored the original initial-exec setting, rebuilt, and re-ran version/decode checks.
- Observed result:
  - global-dynamic: version exited 0; single-thread decode exited 0.
  - restored initial-exec: version exited 0; single-thread decode exited 0.
  - multi-thread decode remains crashing in current Option A.
  - Conclusion: initial-exec is not currently required for version or single-thread decode success in this state.

## 2026-03-22T03:25:00Z
- Intent: Remove partition-alloc initial-exec TLS model override and update docs to reflect that it is not required for current baseline invocation success.
- Commands:
  - Removed the x86_64 partition-alloc TLS model override block from approaches/ia2-a/runtime/partition-alloc/CMakeLists.txt.
  - Updated approaches/ia2-a/docs/tls-unblock-three-approaches.md to mark initial-exec as a tested historical option, not required.
  - Updated approaches/ia2-a/docs/tls-unblock-comprehensive-report.md to explicitly record A/B evidence and optional status.
  - Rebuilt IA2 and dav1d-a artifacts and reran runtime checks.
- Observed result:
  - dav1d --version exited 0.
  - single-thread decode exited 0.
  - multi-thread decode still crashes in current Option A.

## 2026-03-22T03:30:10Z
- Intent: Reformat section 4 of the comprehensive report per user request.
- Commands:
  - Edited approaches/ia2-a/docs/tls-unblock-comprehensive-report.md.
- Observed result:
  - Section 4 was converted from a markdown table to a numbered problem-by-problem list.
  - No content changes to the listed problems, evidence, mitigation, or status.
