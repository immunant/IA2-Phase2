/*
 * Library compartment that triggers dynamic loading through iconv
 */

#include <ia2.h>
#include <ia2_allocator.h>
#include <iconv.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "library.h"

#define IA2_COMPARTMENT 2
#include <ia2_compartment_init.inc>

int trigger_iconv_dlopen(void) {
    /* Removed printf to avoid SEGV_PKUERR from accessing stdout in compartment 2 */

    /* This call will:
     * 1. Enter iconv_open with PKRU for compartment 2
     * 2. Transition to compartment 1 (via callgate since iconv is libc)
     * 3. iconv_open loads conversion modules
     * 4. _dl_debug_state gets called with compartment 1's PKRU (inherited)
     */
    iconv_t converter = iconv_open("UTF-8", "ISO-8859-1");

    if (converter == (iconv_t)-1) {
        /* Can't use printf from compartment 2 when libc is protected */
        return -1;
    }

    /* Do a simple conversion to verify it works */
    const char* input = "test";

    /* Allocate buffers in shared memory so iconv can access them */
    char* shared_input = shared_malloc(strlen(input) + 1);
    char* shared_output = shared_malloc(100);
    strcpy(shared_input, input);

    char* inptr = shared_input;
    char* outptr = shared_output;
    size_t inleft = strlen(input);
    size_t outleft = 100;

    /* FIX for pointer-to-pointer issue:
     * iconv takes pointer-to-pointer parameters (char **inbuf, char **outbuf)
     * which normally live on compartment 2's stack. When iconv (in compartment 1)
     * tries to dereference these pointers, it cannot access compartment 2's stack.
     *
     * SOLUTION: Use shared memory (pkey 0) for the pointer containers.
     * This allows iconv in compartment 1 to access and modify these pointers.
     */

    /* Allocate shared memory for pointer containers */
    char **shared_inptr = shared_malloc(sizeof(char*));
    char **shared_outptr = shared_malloc(sizeof(char*));
    size_t *shared_inleft = shared_malloc(sizeof(size_t));
    size_t *shared_outleft = shared_malloc(sizeof(size_t));

    /* Copy local values to shared memory */
    *shared_inptr = inptr;
    *shared_outptr = outptr;
    *shared_inleft = inleft;
    *shared_outleft = outleft;

    /* Call iconv with shared memory pointers */
    size_t result = iconv(converter, shared_inptr, shared_inleft,
                         shared_outptr, shared_outleft);

    /* Copy results back from shared memory */
    inptr = *shared_inptr;
    outptr = *shared_outptr;
    inleft = *shared_inleft;
    outleft = *shared_outleft;

    /* Free shared memory */
    shared_free(shared_inptr);
    shared_free(shared_outptr);
    shared_free(shared_inleft);
    shared_free(shared_outleft);

    if (result != (size_t)-1) {
        *outptr = '\0';
        /* Success - conversion worked */
    }

    /* Free the shared buffers */
    shared_free(shared_input);
    shared_free(shared_output);

    iconv_close(converter);
    /* Operations complete */

    return 0;
}

void test_compartment_boundary(void) {
    /* Can't use printf from compartment 2 when libc is protected */
    /* Function in compartment 2 */
}