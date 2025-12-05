#include <ia2.h>
#include <ia2_allocator.h>
#include <iconv.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <link.h>
#include "library.h"

#define IA2_COMPARTMENT 2
#include <ia2_compartment_init.inc>

int trigger_iconv_dlopen(void) {
    iconv_t converter = iconv_open("UTF-8", "ISO-8859-1");

    if (converter == (iconv_t)-1) {
        return -1;
    }

    const char* input = "test";
    char* shared_input = shared_malloc(strlen(input) + 1);
    char* shared_output = shared_malloc(100);
    strcpy(shared_input, input);

    char* inptr = shared_input;
    char* outptr = shared_output;
    size_t inleft = strlen(input);
    size_t outleft = 100;

    // iconv takes char** parameters which must be in shared memory
    char **shared_inptr = shared_malloc(sizeof(char*));
    char **shared_outptr = shared_malloc(sizeof(char*));
    size_t *shared_inleft = shared_malloc(sizeof(size_t));
    size_t *shared_outleft = shared_malloc(sizeof(size_t));

    *shared_inptr = inptr;
    *shared_outptr = outptr;
    *shared_inleft = inleft;
    *shared_outleft = outleft;

    size_t result = iconv(converter, shared_inptr, shared_inleft,
                         shared_outptr, shared_outleft);

    inptr = *shared_inptr;
    outptr = *shared_outptr;
    inleft = *shared_inleft;
    outleft = *shared_outleft;

    shared_free(shared_inptr);
    shared_free(shared_outptr);
    shared_free(shared_inleft);
    shared_free(shared_outleft);

    if (result != (size_t)-1) {
        *outptr = '\0';
    }

    shared_free(shared_input);
    shared_free(shared_output);

    iconv_close(converter);

    return 0;
}

void test_compartment_boundary(void) {
}

// Probe to test if plugin compartment can write to ld.so's _r_debug structure
// This tests whether ld.so/libc isolation is working correctly
//
// When IA2_PROBE_LDSO=1 is set:
//   Expected: Process terminates with SIGSEGV (protection key fault) when attempting
//             to write _r_debug from compartment 2 (which blocks pkey 1).
//   Test harness: loader_isolation_faults test forks a child, sets the env var,
//                 and asserts the child exits via SIGSEGV.
void probe_loader_isolation(void) {
    // Only run if IA2_PROBE_LDSO environment variable is set
    const char *probe_env = getenv("IA2_PROBE_LDSO");
    if (!probe_env || probe_env[0] != '1') {
        return;  // Skip probe
    }

    // Attempt to find _r_debug symbol from ld.so
    struct r_debug *ldso_debug =
        (struct r_debug *)dlvsym(RTLD_DEFAULT, "_r_debug", "GLIBC_2.2.5");

    if (!ldso_debug) {
        // Can't find symbol - test cannot proceed
        // We'll just return; main will detect if we didn't crash
        return;
    }

    // This is the critical test: attempt to WRITE to _r_debug->r_state
    // Expected behavior with proper isolation:
    //   - _r_debug is in ld.so's memory, protected by pkey 1
    //   - This compartment (2) runs with PKRU=0xffffffcc which blocks pkey 1
    //   - Therefore this write should trigger a protection key fault and the process
    //     will be terminated by SIGSEGV (caught by the test harness)

    // Attempt the write - this should fault if isolation is working
    ldso_debug->r_state = 0xabad1dea;

    // If we reach here, the write SUCCEEDED - loader isolation is broken!
    // The test harness will detect this and fail the test
}
