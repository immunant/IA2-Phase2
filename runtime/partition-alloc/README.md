# Partition Allocator

This directory contains the partition allocator sources and a simplified shim for overriding libc
symbols using [`ld --wrap`][wrap] based off of the [existing chromium shim][shim]. See the
[Unified malloc shim layer doc][design-doc] for more info. To replace malloc & friends with partition
allocator, first build `libpartition_alloc.so` with the CMake `partition-alloc` target. Then link
your program against `libpartition_alloc.so` with the following linker flags:

```
-Wl,--wrap=calloc
-Wl,--wrap=free
-Wl,--wrap=malloc
-Wl,--wrap=memalign
-Wl,--wrap=posix_memalign
-Wl,--wrap=pvalloc
-Wl,--wrap=realloc
-Wl,--wrap=valloc
-Wl,--wrap=malloc_usable_size
-Wl,--wrap=realpath
-Wl,--wrap=strdup
-Wl,--wrap=strndup
-Wl,--wrap=getcwd
-Wl,--wrap=asprintf
-Wl,--wrap=vasprintf
```

## Partition Address Spaces

This version of partition-alloc makes use of the thread-isolated address pools
in order to isolate allocations for different compartments. See [the
partition-alloc docs][glossary] for information about address pools and how they
are used.

[wrap]: https://chromium.googlesource.com/chromium/src/base/+/refs/heads/main/allocator/allocator_shim_override_libc_symbols.h
[shim]: https://chromium.googlesource.com/chromium/src/base/+/refs/heads/main/allocator/allocator_shim_default_dispatch_to_partition_alloc.cc
[design-doc]: https://docs.google.com/document/d/1yKlO1AO4XjpDad9rjcBOI15EKdAGsuGO_IeZy0g0kxo/edit
[glossary]: https://chromium.googlesource.com/chromium/src/+/refs/heads/main/base/allocator/partition_allocator/glossary.md#pool
