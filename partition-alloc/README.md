# Partition Allocator

This directory contains the partition allocator sources and a simplified shim for overriding libc
symbols using [`ld --wrap`][wrap] based off of the [existing chromium shim][shim]. See the
[Unified malloc shim layer doc][design-doc] for more info. To replace malloc & friends with partition
allocator, first build `libpartition_alloc.so` with the CMake `partition-alloc` target. Then link
your program against `libpartition_alloc.so` with the following linker flags.

```
-Wl,--wrap=calloc  \
-Wl,--wrap=malloc  \
-Wl,--wrap=realloc \
-Wl,--wrap=free
```

[wrap]: https://chromium.googlesource.com/chromium/src/base/+/refs/heads/main/allocator/allocator_shim_override_libc_symbols.h
[shim]: https://chromium.googlesource.com/chromium/src/base/+/refs/heads/main/allocator/allocator_shim_default_dispatch_to_partition_alloc.cc
[design-doc]: https://docs.google.com/document/d/1yKlO1AO4XjpDad9rjcBOI15EKdAGsuGO_IeZy0g0kxo/edit
