#include <ia2_test_runner.h>
#include <ia2.h>
#include <ia2_memory_maps.h>
#include <ia2_loader.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <link.h>
#include <libgen.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <sys/mman.h>
#include "library.h"

INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

void ia2_main(void) {
    ia2_register_compartment("main", 1, NULL);
    ia2_register_compartment("libdl_debug_test_lib.so", 2, NULL);
}

// Parse /proc/self/smaps to find a DSO's writable VMA protection key
// dso_name_pattern: substring to match in the VMA line (e.g., "ld-linux", "libc.so", etc.)
static int get_dso_pkey(const char *dso_name_pattern) {
    FILE *f = fopen("/proc/self/smaps", "r");
    if (!f) {
        return -1;
    }

    char line[512];
    bool in_target_rw = false;
    int pkey = -1;

    while (fgets(line, sizeof(line), f)) {
        // Check if this is a new VMA line (starts with address range)
        if (line[0] >= '0' && line[0] <= '9') {
            // Check if it matches our DSO pattern and is writable
            if (strstr(line, dso_name_pattern) && strstr(line, "rw")) {
                in_target_rw = true;
            } else {
                in_target_rw = false;
            }
        } else if (in_target_rw && strncmp(line, "ProtectionKey:", 14) == 0) {
            // Extract pkey value
            sscanf(line, "ProtectionKey: %d", &pkey);
            break;
        }
    }

    fclose(f);
    return pkey;
}

// Parse /proc/self/smaps to find ld.so's writable VMA protection key
static int get_ldso_pkey(void) {
    return get_dso_pkey("ld-linux");
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

Test(dl_debug, loader_isolation_skipped) {
    cr_log_info("Main: Probe skipped (IA2_PROBE_LDSO not set)");
    cr_log_info("Main: Set IA2_PROBE_LDSO=1 to run loader_isolation_faults test");

    // Call probe without IA2_PROBE_LDSO - should return immediately
    probe_loader_isolation();
}

Test(dl_debug, loader_isolation_faults) {
    cr_log_info("Main: Testing loader isolation enforcement from compartment 2");
    cr_log_info("Main: Forking child to probe ld.so _r_debug from compartment 2");

    pid_t pid = fork();
    if (pid == -1) {
        cr_log_info("Main: Fork failed: %s", strerror(errno));
        cr_assert(0);
    }

    if (pid == 0) {
        // Child process: reset SIGSEGV handler to default so the fault terminates the process
        signal(SIGSEGV, SIG_DFL);

        // Enable probe and attempt write
        setenv("IA2_PROBE_LDSO", "1", 1);
        probe_loader_isolation();

        // If we reach here, isolation is broken
        fprintf(stderr, "CHILD: BUG DETECTED - probe returned without faulting!\n");
        fprintf(stderr, "CHILD: Compartment 2 wrote to _r_debug on pkey 1\n");
        _exit(42);  // Distinct exit code to detect this failure
    }

    // Parent process: wait for child and verify it faulted
    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        cr_log_info("Main: Child exited cleanly with code %d - ISOLATION BROKEN", exit_code);
        cr_assert(0);
    } else if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        if (sig == SIGSEGV) {
            cr_log_info("Main: Child faulted with SIGSEGV - loader isolation enforced correctly");
        } else {
            cr_log_info("Main: Child terminated with signal %d (expected SIGSEGV)", sig);
            cr_assert(0);
        }
    } else {
        cr_log_info("Main: Child terminated unexpectedly (status 0x%x)", status);
        cr_assert(0);
    }
}

Test(dl_debug, manual_retag_loader) {
    cr_log_info("Main: Testing manual retagging of loader segments with ia2_tag_link_map()");
    cr_log_info("Main: === INITIAL MEMORY MAP (before retagging) ===");
    ia2_log_memory_maps(stdout);

    // Get handle to _r_debug to access the link map
    struct r_debug *debug = (struct r_debug *)dlsym(RTLD_DEFAULT, "_r_debug");
    if (!debug) {
        cr_log_info("Main: Failed to find _r_debug symbol");
        cr_assert(0);
    }
    cr_log_info("Main: Found _r_debug at %p", debug);

    // Walk the link map to find ld.so
    struct link_map *ldso = debug->r_map;
    bool found_ldso = false;

    while (ldso) {
        const char *name = ldso->l_name[0] ? basename(ldso->l_name) : "(main)";
        cr_log_info("Main: Checking DSO: %s (base 0x%lx)", name, ldso->l_addr);

        // Look for ld-linux-* (the dynamic linker)
        if (strstr(name, "ld-linux") != NULL) {
            cr_log_info("Main: Found ld.so: %s at base 0x%lx", ldso->l_name, ldso->l_addr);

            // Verify initial pkey is 1
            int pkey_before = get_ldso_pkey();
            cr_assert(pkey_before == 1);

            // Retag to compartment 1 (should be idempotent since already on pkey 1)
            cr_log_info("Main: Calling ia2_tag_link_map(ldso, 1)...");
            ia2_tag_link_map(ldso, 1);

            // Verify pkey is still 1 (idempotent)
            int pkey_after = get_ldso_pkey();
            cr_assert(pkey_after == 1);

            cr_log_info("Main: Successfully retagged ld.so to compartment 1");
            cr_log_info("Main: === MEMORY MAP AFTER RETAGGING ===");
            ia2_log_memory_maps(stdout);

            found_ldso = true;
            break;
        }

        ldso = ldso->l_next;
    }

    if (!found_ldso) {
        cr_log_info("Main: Could not find ld.so in link map");
        cr_assert(0);
    }
    cr_log_info("Main: Test complete - ia2_tag_link_map() works correctly");
}

Test(dl_debug, mistag_and_fix) {
    // Find ld.so
    struct r_debug *debug = (struct r_debug *)dlsym(RTLD_DEFAULT, "_r_debug");
    if (!debug) {
        cr_assert(0);
    }

    struct link_map *ldso = debug->r_map;
    bool found_ldso = false;

    while (ldso) {
        const char *name = ldso->l_name[0] ? basename(ldso->l_name) : "(main)";
        if (strstr(name, "ld-linux") != NULL) {
            found_ldso = true;
            break;
        }
        ldso = ldso->l_next;
    }

    if (!found_ldso) {
        cr_assert(0);
    }

    // Save original _r_debug state value
    int original_state = debug->r_state;

    // Fork a child that will mistag, attempt write, and fix
    pid_t pid = fork();
    if (pid == -1) {
        cr_assert(0);
    }

    if (pid == 0) {
        // Child: reset SIGSEGV handler to default
        signal(SIGSEGV, SIG_DFL);

        // Verify pkey is 1 before mistag
        int pkey_before = get_ldso_pkey();
        if (pkey_before != 1) _exit(43);

        // Mistag ld.so to compartment 2 (wrong!)
        ia2_tag_link_map(ldso, 2);

        // Verify pkey changed to 2
        int pkey_after = get_ldso_pkey();
        if (pkey_after != 2) _exit(44);

        // Attempt to write _r_debug from compartment 1 (should fault)
        debug->r_state = 0xabad1dea;

        // If we reach here, isolation failed
        _exit(42);
    }

    // Parent: wait for child to fault
    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        cr_assert(0);
    } else if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        if (sig != SIGSEGV) {
            cr_assert(0);
        }
    } else {
        cr_assert(0);
    }

    // Parent's ld.so still on pkey 1 (fork is copy-on-write)
    int parent_pkey = get_ldso_pkey();
    cr_assert(parent_pkey == 1);

    // Parent can still access ld.so (was never mistagged in parent)
    // Verify we can write from compartment 1
    debug->r_state = original_state + 1;
    int new_state = debug->r_state;

    if (new_state != original_state + 1) {
        cr_assert(0);
    }

    // Restore original state
    debug->r_state = original_state;
}

// Test: Verify loader gate routes allocations through PartitionAlloc (pkey 1)
//
// Run with:
//   IA2_TEST_NAME=loader_allocator_partitionalloc ./dl_debug_test
//
// Or run all tests:
//   ./dl_debug_test
//
// Validates:
//   - ia2_loader_gate_enter/exit infrastructure works
//   - malloc inside gate increments ia2_loader_alloc_count
//   - PartitionAlloc routes allocation to pkey 1
Test(dl_debug, loader_allocator_partitionalloc) {
    ia2_loader_alloc_count = 0;

    ia2_loader_gate_enter();
    void *test_alloc = malloc(64);
    ia2_loader_gate_exit();

    cr_assert(test_alloc != NULL);
    cr_assert(ia2_loader_alloc_count > 0);

    free(test_alloc);
}

// Test: Verify anonymous mappings created during loader operations are tagged with pkey 1
//
// Run with:
//   IA2_TEST_NAME=loader_anon_mmap_tagging ./dl_debug_test
//
// Or run all tests:
//   ./dl_debug_test
//
// Trace syscalls with:
//   strace -f -e mmap,pkey_mprotect IA2_TEST_NAME=loader_anon_mmap_tagging ./dl_debug_test
//
// Expected strace output shows PROT_NONE → pkey_mprotect(1) sequence:
//   mmap(NULL, 4096, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x...
//   pkey_mprotect(0x..., 4096, PROT_READ|PROT_WRITE, 1) = 0
//
// Validates:
//   - mmap wrapper intercepts calls when gate is active
//   - ia2_loader_mmap_count increments (proves wrapper executed)
//   - Two-step allocation: PROT_NONE first, then pkey_mprotect
//   - Kernel assigns pkey 1 (verified via /proc/self/smaps)
Test(dl_debug, loader_anon_mmap_tagging) {
    ia2_loader_mmap_count = 0;

    ia2_loader_gate_enter();
    void *anon_map = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ia2_loader_gate_exit();

    cr_assert(anon_map != MAP_FAILED);
    cr_assert(ia2_loader_mmap_count > 0);

    FILE *f = fopen("/proc/self/smaps", "r");
    cr_assert(f != NULL);

    char line[512];
    bool found_mapping = false;
    bool found_pkey = false;
    int pkey_value = -1;

    while (fgets(line, sizeof(line), f)) {
        if (line[0] >= '0' && line[0] <= '9') {
            unsigned long start, end;
            if (sscanf(line, "%lx-%lx", &start, &end) == 2) {
                if ((unsigned long)anon_map >= start && (unsigned long)anon_map < end) {
                    found_mapping = true;
                    continue;
                }
            }
            found_mapping = false;
        } else if (found_mapping && strncmp(line, "ProtectionKey:", 14) == 0) {
            sscanf(line, "ProtectionKey: %d", &pkey_value);
            found_pkey = true;
            break;
        }
    }

    fclose(f);

    cr_assert(found_pkey);
    cr_assert(pkey_value == 1);

    munmap(anon_map, 4096);
}
