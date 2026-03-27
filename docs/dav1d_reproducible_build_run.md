# Reproducible `dav1d` Build And Run (IA2)

Date: 2026-03-26

This is the exact flow used to get reproducible results for:
- `dav1d --version`
- single-thread decode (`--threads 1`)

## 1. Inputs

- IA2 tree (example): `/home/davidanekstein/immunant/approaches/ia2-cmp-main`
- IA2 build dir: `/home/davidanekstein/immunant/approaches/ia2-cmp-main/build/x86_64`
- dav1d tree: `/home/davidanekstein/immunant/dav1d-ia2-ia2`
- test input: `/home/davidanekstein/immunant/test.ivf`

## 2. Build IA2 Runtime Artifacts

```sh
ninja -C /home/davidanekstein/immunant/approaches/ia2-cmp-main/build/x86_64 \
  runtime/libia2/liblibia2.a \
  runtime/libia2/libc.so.6 \
  runtime/libia2/ld-linux-x86-64.so.2 \
  runtime/partition-alloc/libpartition-alloc.so \
  tools/pad-tls/pad-tls
```

## 3. Configure And Build dav1d

```sh
meson setup /home/davidanekstein/immunant/dav1d-ia2-ia2/build/x86_64_cmp_main2 \
  /home/davidanekstein/immunant/dav1d-ia2-ia2 \
  -Dbuildtype=debug \
  -Dia2_enable=true \
  -Dia2_libc_compartment=true \
  -Dia2_permissive_mode=false \
  -Dia2_path=/home/davidanekstein/immunant/approaches/ia2-cmp-main \
  -Dia2_build_path=/home/davidanekstein/immunant/approaches/ia2-cmp-main/build/x86_64

ninja -C /home/davidanekstein/immunant/dav1d-ia2-ia2/build/x86_64_cmp_main2 tools/dav1d
```

## 4. Required Manual Step: Build `libcallgates.so`

Meson links `-lcallgates` but does not auto-produce it here.
Build it into the build `src/` directory:

```sh
cc -shared -fPIC -Wl,-z,now \
  /home/davidanekstein/immunant/dav1d-ia2-ia2/callgate_wrapper.c \
  -I /home/davidanekstein/immunant/approaches/ia2-cmp-main/runtime/libia2/include \
  -o /home/davidanekstein/immunant/dav1d-ia2-ia2/build/x86_64_cmp_main2/src/libcallgates.so

cp -f /home/davidanekstein/immunant/approaches/ia2-cmp-main/build/x86_64/runtime/partition-alloc/libpartition-alloc.so \
  /home/davidanekstein/immunant/dav1d-ia2-ia2/build/x86_64_cmp_main2/src/libpartition-alloc.so
```

## 5. Required Manual Step: `pad-tls`

Without this, execution can fail immediately in `__wrap_main` due to TLS page protection layout.

Copy DSOs into `build/.../src` and pad them in place:

```sh
B=/home/davidanekstein/immunant/dav1d-ia2-ia2/build/x86_64_cmp_main2
P=/home/davidanekstein/immunant/approaches/ia2-cmp-main/build/x86_64/tools/pad-tls/pad-tls

cp -f /home/davidanekstein/immunant/approaches/ia2-cmp-main/build/x86_64/runtime/libia2/libc.so.6 $B/src/libc.so.6
cp -f /lib/x86_64-linux-gnu/libm.so.6 $B/src/libm.so.6
cp -f /usr/lib/x86_64-linux-gnu/libstdc++.so.6 $B/src/libstdc++.so.6
cp -f /usr/lib/x86_64-linux-gnu/libgcc_s.so.1 $B/src/libgcc_s.so.1

for f in \
  $B/tools/dav1d \
  $B/src/libcallgates.so \
  $B/src/libdav1d.so.7.0.0 \
  $B/src/libpartition-alloc.so \
  $B/src/libc.so.6 \
  $B/src/libm.so.6 \
  $B/src/libstdc++.so.6 \
  $B/src/libgcc_s.so.1
 do
  "$P" --allow-no-tls "$f"
 done
```

## 6. Run

```sh
/home/davidanekstein/immunant/dav1d-ia2-ia2/build/x86_64_cmp_main2/tools/dav1d --version

/home/davidanekstein/immunant/dav1d-ia2-ia2/build/x86_64_cmp_main2/tools/dav1d \
  -i /home/davidanekstein/immunant/test.ivf \
  -o /dev/null \
  --muxer null \
  --threads 1
```

## 7. GDB Fault Capture (Decode)

```sh
gdb -q /home/davidanekstein/immunant/dav1d-ia2-ia2/build/x86_64_cmp_main2/tools/dav1d
```

Inside gdb:

```gdb
set pagination off
set args -i /home/davidanekstein/immunant/test.ivf -o /dev/null --muxer null --threads 1
run
info registers pkru
bt
```

## 8. Notes

- `--version` and decode can have different behavior.
- For side-by-side commit comparisons, use separate IA2 worktrees and separate dav1d build dirs.
- Current crash analysis for `main` vs `da780` is documented in `docs/dav1d_fault_addresses_first_principles.md`.
