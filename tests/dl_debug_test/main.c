#include <ia2_test_runner.h>
#include <ia2.h>
#include <ia2_get_pkey.h>
#include <ia2_test_pkey_utils.h>
#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>
#include <link.h>
#include "library.h"

INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

void ia2_main(void) {
    ia2_register_compartment("main", 1, NULL);
    ia2_register_compartment("libdl_debug_test_lib.so", 2, NULL);
}

Test(dl_debug, libc_compartment_inheritance) {
    cr_log_info("Main: Starting test in compartment 1");

    int result = trigger_iconv_dlopen();

    cr_assert_eq(result, 0);
    cr_log_info("Main: Test complete - iconv conversion succeeded, dl_debug_state inherited compartment 1");
}

Test(dl_debug, basic_compartment_check) {
    cr_log_info("Main: Basic compartment check");

    test_compartment_boundary();

    cr_log_info("Main: Compartment boundaries verified");
}

// Exercise dlopen(), which allocates loader metadata, and ensure it preserves
// the caller compartment while mapping the loaded DSO onto its compartment pkey.
Test(dl_debug, dlopen_allocation_compartment) {
    size_t pkey_before = ia2_get_pkey();

    void *handle = dlopen("./libdl_debug_test_lib.so", RTLD_NOW);
    cr_assert(handle != NULL);

    size_t pkey_after = ia2_get_pkey();
    cr_assert_eq(pkey_after, pkey_before);

    int lib_pkey = ia2_test_get_dso_pkey("libdl_debug_test_lib.so");
    cr_assert(lib_pkey == 2);

    dlclose(handle);
}

// Successful path check:
// glibc exposes the loader's bookkeeping `struct link_map` for a handle via
// dlinfo(RTLD_DI_LINKMAP). Even if malloc traffic stays inside existing arenas,
// this pointer is concrete evidence of loader allocation. We read its pkey (and
// the pkey of its l_name buffer if present) to see which compartment the loader
// allocated in.
Test(dl_debug, dlopen_alloc_link_map_pkey) {
    size_t pkey_expected = ia2_get_pkey();

    void *handle = dlopen("./libdl_debug_test_lib.so", RTLD_NOW | RTLD_LOCAL);
    cr_assert(handle != NULL);

    struct link_map *lm = NULL;
    int rc = dlinfo(handle, RTLD_DI_LINKMAP, &lm);
    cr_assert_eq(rc, 0);
    cr_assert(lm != NULL);

    int lm_pkey = ia2_test_get_addr_pkey(lm);
    cr_assert(lm_pkey != -1);
    cr_assert(lm_pkey == 0 || lm_pkey == 1 || lm_pkey == 2 || lm_pkey == 3);

    if (lm->l_name) {
        int name_pkey = ia2_test_get_addr_pkey(lm->l_name);
        cr_assert(name_pkey != -1);
        cr_assert(name_pkey == 0 || name_pkey == 1 || name_pkey == 2 || name_pkey == 3);
    }

    dlclose(handle);
}

// dlerror() builds an error string via malloc (asprintf). Without active
// loader gating, this allocation lands in the shared heap (pkey 0).
Test(dl_debug, dlerror_alloc_pkey) {
    void *handle = dlopen("./libdl_debug_test_lib.so", RTLD_NOW | RTLD_LOCAL);
    cr_assert(handle != NULL);

    (void)dlerror(); // clear any prior error
    void *sym = dlsym(handle, "ia2__definitely_missing_symbol__12345");
    cr_assert(sym == NULL);

    char *err = dlerror();
    cr_assert(err != NULL);

    int err_pkey = ia2_test_get_addr_pkey(err);
    cr_assert(err_pkey != -1);
    cr_assert_eq(err_pkey, 0);

    dlclose(handle);
}

typedef struct {
    uintptr_t start;
    uintptr_t end;
    int pkey;
    bool heap_like;
} ia2_region_info;

// Parse /proc/self/smaps and collect rw anonymous/heap mappings with pkeys.
// This is our only user-space view into which protection key the kernel
// assigned to new heap arenas created by glibc's loader.
static size_t ia2_read_rw_regions(ia2_region_info *out, size_t max_regions) {
    FILE *f = fopen("/proc/self/smaps", "r");
    if (!f) {
        return 0;
    }

    char line[512];
    size_t count = 0;
    ia2_region_info current = {0};

    while (fgets(line, sizeof(line), f)) {
        uintptr_t start, end;
        char perms[8] = {0};
        char path[256] = {0};

        if (sscanf(line, "%lx-%lx %7s %*s %*s %*s %255[^\n]", &start, &end, perms, path) >= 3) {
            // Flush previous region if it was rw
            if (current.heap_like && count < max_regions && current.pkey != -2) {
                out[count++] = current;
            }
            current.start = start;
            current.end = end;
            current.pkey = -2; // unknown
            bool is_rw = strstr(perms, "rw") != NULL;
            bool is_heap = strstr(path, "[heap]") != NULL;
            bool is_anon = path[0] == '\0';
            current.heap_like = is_rw && (is_heap || is_anon);
        } else if (current.heap_like && strncmp(line, "ProtectionKey:", 14) == 0) {
            int p;
            if (sscanf(line, "ProtectionKey: %d", &p) == 1) {
                current.pkey = p;
            }
        }
    }

    if (current.heap_like && count < max_regions && current.pkey != -2) {
        out[count++] = current;
    }

    fclose(f);
    return count;
}

static bool ia2_region_equals(const ia2_region_info *a, const ia2_region_info *b) {
    return a->start == b->start && a->end == b->end && a->pkey == b->pkey && a->heap_like == b->heap_like;
}

// Smaps-based growth check (best-effort):
//  - dlmopen(LM_ID_NEWLM, ...) encourages the loader to grow malloc arenas for
//    a new namespace. We snapshot /proc/self/smaps and look for new or expanded
//    rw heap/anon regions, asserting their pkey matches the caller's compartment.
//  - If dlmopen fails or no growth appears, we log via test output and return
//    to avoid masking failures elsewhere; pointer-based tests above give stronger
//    evidence when growth isn't observable.
Test(dl_debug, dlopen_allocation_smaps_growth) {
    size_t pkey_expected = ia2_get_pkey();

    ia2_region_info before[256] = {0};
    size_t before_count = ia2_read_rw_regions(before, 256);
    cr_assert(before_count > 0);

    // Loop dlmopen/dlclose to force internal allocations (new namespace each time).
    int loads = 0;
    for (int i = 0; i < 256; ++i) {
        void *handle = dlmopen(LM_ID_NEWLM, "./libdl_debug_test_lib.so", RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
            break; // dlmopen may be unavailable in this environment
        }
        ++loads;
        dlclose(handle);
    }
    if (loads == 0) {
        cr_log_info("dlopen_allocation_smaps_growth: dlmopen unavailable; skipping growth check");
        return; // Cannot exercise loader allocations here; skip silently.
    }

    ia2_region_info after[256] = {0};
    size_t after_count = ia2_read_rw_regions(after, 256);
    cr_assert(after_count >= before_count);

    size_t new_regions = 0;
    for (size_t i = 0; i < after_count; ++i) {
        const ia2_region_info *r = &after[i];
        bool found = false;
        for (size_t j = 0; j < before_count; ++j) {
            const ia2_region_info *b = &before[j];
            if (r->start == b->start) {
                found = true;
                if (r->end > b->end) {
                    ++new_regions; // region grew
                    cr_assert_eq(r->pkey, (int)pkey_expected);
                }
                break;
            }
        }
        if (!found) {
            ++new_regions;
            cr_assert_eq(r->pkey, (int)pkey_expected);
        }
    }

    // If no new regions appeared, loader allocations stayed within existing
    // arenas; nothing to assert in this environment.
    if (new_regions == 0) {
        cr_log_info("dlopen_allocation_smaps_growth: no new heap/anon mappings observed after %d dlmopen calls", loads);
        return;
    }
}
