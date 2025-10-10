#define _GNU_SOURCE
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdatomic.h>
#include <stdarg.h>
#include <ia2_loader.h>

// Forward declarations for real symbols
extern void *__real_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
extern void *__real_mmap64(void *addr, size_t length, int prot, int flags, int fd, off64_t offset);
extern void *__real_mremap(void *old_address, size_t old_size, size_t new_size, int flags, ...);

// pkey_mprotect syscall (not always in libc headers)
#ifndef SYS_pkey_mprotect
#define SYS_pkey_mprotect 329
#endif

static int pkey_mprotect_syscall(void *addr, size_t len, int prot, int pkey) {
    return syscall(SYS_pkey_mprotect, addr, len, prot, pkey);
}

// Wrapped mmap: tag anonymous mappings with pkey 1 when loader gate is active
void *__wrap_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    // Fast path: gate not active, use real mmap
    if (!ia2_in_loader_gate) {
        return __real_mmap(addr, length, prot, flags, fd, offset);
    }

    // Loader gate active: create mapping with PROT_NONE first, then retag
    void *result = __real_mmap(addr, length, PROT_NONE, flags, fd, offset);
    if (result == MAP_FAILED) {
        return result;
    }

    // Apply original protections with pkey 1
    if (pkey_mprotect_syscall(result, length, prot, 1) != 0) {
        // Failed to set pkey - unmap to avoid leaking wrongly-tagged memory
        munmap(result, length);
        return MAP_FAILED;
    }

    // Successfully tagged - increment counter
    atomic_fetch_add(&ia2_loader_mmap_count, 1);
    return result;
}

// Wrapped mmap64: same logic as mmap
void *__wrap_mmap64(void *addr, size_t length, int prot, int flags, int fd, off64_t offset) {
    if (!ia2_in_loader_gate) {
        return __real_mmap64(addr, length, prot, flags, fd, offset);
    }

    void *result = __real_mmap64(addr, length, PROT_NONE, flags, fd, offset);
    if (result == MAP_FAILED) {
        return result;
    }

    if (pkey_mprotect_syscall(result, length, prot, 1) != 0) {
        munmap(result, length);
        return MAP_FAILED;
    }

    atomic_fetch_add(&ia2_loader_mmap_count, 1);
    return result;
}

// Wrapped mremap: tag new/expanded regions with pkey 1 when loader gate is active
void *__wrap_mremap(void *old_address, size_t old_size, size_t new_size, int flags, ...) {
    // Extract optional new_address argument if MREMAP_FIXED is set
    void *new_address = NULL;
    if (flags & MREMAP_FIXED) {
        va_list args;
        va_start(args, flags);
        new_address = va_arg(args, void *);
        va_end(args);
    }

    // Fast path: gate not active
    if (!ia2_in_loader_gate) {
        if (flags & MREMAP_FIXED) {
            return __real_mremap(old_address, old_size, new_size, flags, new_address);
        } else {
            return __real_mremap(old_address, old_size, new_size, flags);
        }
    }

    // Call real mremap
    void *result;
    if (flags & MREMAP_FIXED) {
        result = __real_mremap(old_address, old_size, new_size, flags, new_address);
    } else {
        result = __real_mremap(old_address, old_size, new_size, flags);
    }

    if (result == MAP_FAILED) {
        return result;
    }

    // If mapping grew, retag the new portion with pkey 1
    // Note: We can't easily determine original protections here, so we assume RW
    // which is typical for anonymous mappings that get remapped
    if (new_size > old_size) {
        size_t growth = new_size - old_size;
        void *new_portion = (char *)result + old_size;

        // Try to retag new portion - if this fails, the mapping is still valid
        // but may not have the correct pkey. This is a best-effort approach.
        if (pkey_mprotect_syscall(new_portion, growth, PROT_READ | PROT_WRITE, 1) == 0) {
            atomic_fetch_add(&ia2_loader_mmap_count, 1);
        }
    }

    return result;
}
