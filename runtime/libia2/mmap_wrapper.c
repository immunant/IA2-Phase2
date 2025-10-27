#define _GNU_SOURCE
#include <sys/mman.h>
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

// Check if a memory region is file-backed (has a pathname in /proc/self/maps)
// Returns 1 if file-backed, 0 if anonymous, -1 on error
static int is_file_backed_mapping(void *addr) {
    FILE *f = fopen("/proc/self/maps", "r");
    if (!f) {
        return -1;
    }

    char line[512];
    unsigned long target = (unsigned long)addr;
    int result = -1;

    while (fgets(line, sizeof(line), f)) {
        unsigned long start, end;
        char perms[5], device[32], rest[256];
        unsigned long offset, inode;

        // Parse: start-end perms offset dev:maj:min inode [pathname]
        int parsed = sscanf(line, "%lx-%lx %4s %lx %31s %lu %255[^\n]",
                           &start, &end, perms, &offset, device, &inode, rest);

        if (parsed >= 6 && target >= start && target < end) {
            // File-backed mappings have either:
            // 1. A non-zero inode with a pathname (parsed == 7)
            // 2. Device other than 00:00
            if (strcmp(device, "00:00") != 0 || (parsed == 7 && rest[0] != '[')) {
                result = 1;  // File-backed
            } else {
                result = 0;  // Anonymous
            }
            break;
        }
    }

    fclose(f);
    return result;
}

// Common helper: retag anonymous loader mapping with pkey 1
// Called after successful mmap/mmap64 of an anonymous region during loader operations
// Returns MAP_FAILED on pkey_mprotect failure (with mapping cleaned up), otherwise returns result
static void *ia2_retag_anon_loader_mmap(void *result, size_t length, int prot) {
    if (result == MAP_FAILED) {
        return result;
    }

    // Apply original protections with pkey 1
    if (pkey_mprotect(result, length, prot, 1) != 0) {
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

// Wrapped mmap: tag anonymous mappings with pkey 1 when loader gate is active
void *__wrap_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    // Fast path: gate not active, use real mmap.
    // We cannot rely on PKRU alone here: loader and main share pkey 1 until the
    // loader moves to its own compartment, so a hardware read would report the
    // same value for both. The TLS flag keeps the loader-only retagging path
    // precise even after PKRU gating flips permissions around it.
    if (!ia2_in_loader_gate) {
        return __real_mmap(addr, length, prot, flags, fd, offset);
    }

    // Only retag anonymous mappings - file-backed mappings should inherit
    // the compartment pkey configured by ia2_register_compartment
    if ((flags & MAP_ANONYMOUS) == 0) {
        // File-backed mapping: pass through without retagging
        return __real_mmap(addr, length, prot, flags, fd, offset);
    }

    // Anonymous mapping in loader gate: create mapping with PROT_NONE first,
    // then retag to pkey 1. We cannot choose a pkey at mmap time, so this
    // two-step process prevents exposing loader pages under the caller's pkey
    // before pkey_mprotect() fixes them.
    void *result = __real_mmap(addr, length, PROT_NONE, flags, fd, offset);
    return ia2_retag_anon_loader_mmap(result, length, prot);
}

// Wrapped mmap64: tag anonymous mappings with pkey 1 when loader gate is active
void *__wrap_mmap64(void *addr, size_t length, int prot, int flags, int fd, off64_t offset) {
    // Fast path: gate not active, use real mmap64 (see comment in __wrap_mmap
    // on why we still consult ia2_in_loader_gate despite PKRU gating).
    if (!ia2_in_loader_gate) {
        return __real_mmap64(addr, length, prot, flags, fd, offset);
    }

    // Only retag anonymous mappings - file-backed mappings should inherit
    // the compartment pkey configured by ia2_register_compartment
    if ((flags & MAP_ANONYMOUS) == 0) {
        // File-backed mapping: pass through without retagging
        return __real_mmap64(addr, length, prot, flags, fd, offset);
    }

    // Anonymous mapping in loader gate: create mapping with PROT_NONE first,
    // then retag to pkey 1. Same rationale as mmap() -- avoid a window where the
    // mapping inherits the caller's pkey and bypasses loader isolation.
    void *result = __real_mmap64(addr, length, PROT_NONE, flags, fd, offset);
    return ia2_retag_anon_loader_mmap(result, length, prot);
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

    // Fast path: gate not active (same rationale as mmap/mmap64).
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

    // If mapping grew, retag the new portion with pkey 1, but only for anonymous mappings
    // File-backed mappings should keep their original compartment pkey
    if (new_size > old_size) {
        size_t growth = new_size - old_size;
        void *new_portion = (char *)result + old_size;

        // IMPORTANT: Check the NEW mapping location (result), not old_address!
        // When mremap moves the mapping (MREMAP_MAYMOVE/MREMAP_FIXED), old_address
        // no longer exists in /proc/self/maps, so we must probe the live VMA at result.
        int is_file_backed = is_file_backed_mapping(result);

        if (is_file_backed == 0) {
            // Anonymous mapping: retag growth with pkey 1
            // We must preserve the original protection bits to avoid breaking W^X or
            // making read-only regions writable

            // Query the actual protections from /proc/self/maps
            // The kernel copies protections from the original region when growing
            int prot = get_region_protections(new_portion);

            if (prot >= 0) {
                // Successfully retrieved protections - retag with correct flags
                if (pkey_mprotect(new_portion, growth, prot, 1) == 0) {
                    atomic_fetch_add(&ia2_loader_mmap_count, 1);
                }
                // If pkey_mprotect fails, mapping is still valid but untagged
            }
            // If get_region_protections fails, skip tagging to avoid applying wrong protections
        }
        // If file-backed (is_file_backed == 1) or error (is_file_backed == -1), skip retagging
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
