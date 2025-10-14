#define _GNU_SOURCE
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdatomic.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ia2_loader.h>

// Forward declarations for real symbols
extern void *__real_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
extern void *__real_mmap64(void *addr, size_t length, int prot, int flags, int fd, off64_t offset);
extern void *__real_mremap(void *old_address, size_t old_size, size_t new_size, int flags, ...);

#if defined(__x86_64__)

// pkey_mprotect syscall (not always in libc headers)
#ifndef SYS_pkey_mprotect
#define SYS_pkey_mprotect 329
#endif

static int pkey_mprotect_syscall(void *addr, size_t len, int prot, int pkey) {
    return syscall(SYS_pkey_mprotect, addr, len, prot, pkey);
}

// Query actual protection flags for a memory region from /proc/self/maps
// Returns -1 on error, otherwise returns prot flags (PROT_READ, PROT_WRITE, PROT_EXEC)
static int get_region_protections(void *addr) {
    FILE *f = fopen("/proc/self/maps", "r");
    if (!f) {
        return -1;
    }

    char line[512];
    unsigned long target = (unsigned long)addr;
    int prot = -1;

    while (fgets(line, sizeof(line), f)) {
        unsigned long start, end;
        char perms[5];

        if (sscanf(line, "%lx-%lx %4s", &start, &end, perms) == 3) {
            if (target >= start && target < end) {
                // Parse permission string (e.g., "r-xp", "rw-p")
                prot = 0;
                if (perms[0] == 'r') prot |= PROT_READ;
                if (perms[1] == 'w') prot |= PROT_WRITE;
                if (perms[2] == 'x') prot |= PROT_EXEC;
                break;
            }
        }
    }

    fclose(f);
    return prot;
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
        // Save errno before munmap, which may clobber it
        int saved_errno = errno;
        munmap(result, length);
        errno = saved_errno;
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
        // Save errno before munmap, which may clobber it
        int saved_errno = errno;
        munmap(result, length);
        errno = saved_errno;
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
    // We must preserve the original protection bits to avoid breaking W^X or
    // making read-only regions writable
    if (new_size > old_size) {
        size_t growth = new_size - old_size;
        void *new_portion = (char *)result + old_size;

        // Query the actual protections from /proc/self/maps
        // The kernel copies protections from the original region when growing
        int prot = get_region_protections(new_portion);

        if (prot >= 0) {
            // Successfully retrieved protections - retag with correct flags
            if (pkey_mprotect_syscall(new_portion, growth, prot, 1) == 0) {
                atomic_fetch_add(&ia2_loader_mmap_count, 1);
            }
            // If pkey_mprotect fails, mapping is still valid but untagged
        }
        // If get_region_protections fails, skip tagging to avoid applying wrong protections
    }

    return result;
}

#else  // non-x86_64 builds: pass through to real libc symbols

void *__wrap_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    return __real_mmap(addr, length, prot, flags, fd, offset);
}

void *__wrap_mmap64(void *addr, size_t length, int prot, int flags, int fd, off64_t offset) {
    return __real_mmap64(addr, length, prot, flags, fd, offset);
}

void *__wrap_mremap(void *old_address, size_t old_size, size_t new_size, int flags, ...) {
    void *new_address = NULL;
    if (flags & MREMAP_FIXED) {
        va_list args;
        va_start(args, flags);
        new_address = va_arg(args, void *);
        va_end(args);
        return __real_mremap(old_address, old_size, new_size, flags, new_address);
    }
    return __real_mremap(old_address, old_size, new_size, flags);
}

#endif  // defined(__x86_64__)
