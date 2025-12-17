#ifndef IA2_TEST_PKEY_UTILS_H
#define IA2_TEST_PKEY_UTILS_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Get protection key for a DSO by searching /proc/self/smaps.
// Returns the pkey of the first writable (rw) region matching dso_name_pattern
// or -1 on error/not found.
static inline int ia2_test_get_dso_pkey(const char *dso_name_pattern) {
    FILE *f = fopen("/proc/self/smaps", "r");
    if (!f) return -1;

    char line[512];
    bool in_target_rw = false;
    int pkey = -1;

    while (fgets(line, sizeof(line), f)) {
        if (line[0] >= '0' && line[0] <= '9') {
            // New memory mapping line
            in_target_rw = strstr(line, dso_name_pattern) && strstr(line, "rw");
        } else if (in_target_rw && strncmp(line, "ProtectionKey:", 14) == 0) {
            if (sscanf(line, "ProtectionKey: %d", &pkey) == 1) {
                break;
            }
        }
    }
    fclose(f);
    return pkey;
}

// Get protection key for an arbitrary address by scanning /proc/self/smaps.
// Returns -1 if the mapping or ProtectionKey metadata is missing.
static inline int ia2_test_get_addr_pkey(const void *addr) {
    if (!addr) {
        return -1;
    }

    FILE *f = fopen("/proc/self/smaps", "r");
    if (!f) {
        return -1;
    }

    const uintptr_t target = (uintptr_t)addr;
    char line[512];
    bool in_target = false;

    while (fgets(line, sizeof(line), f)) {
        uintptr_t start, end;
        if (sscanf(line, "%lx-%lx %*s %*s %*s %*s %*s", &start, &end) == 2) {
            in_target = (target >= start && target < end);
        } else if (in_target && strncmp(line, "ProtectionKey:", 14) == 0) {
            int pkey = -1;
            if (sscanf(line, "ProtectionKey: %d", &pkey) == 1) {
                fclose(f);
                return pkey;
            }
            break;
        }
    }

    fclose(f);
    return -1;
}

#endif // IA2_TEST_PKEY_UTILS_H
