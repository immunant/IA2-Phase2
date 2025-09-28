/*
 * Library compartment that triggers dynamic loading through iconv
 */

#include <ia2.h>
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
    char output[100];
    char* inptr = (char*)input;
    char* outptr = output;
    size_t inleft = strlen(input);
    size_t outleft = sizeof(output);

    size_t result = iconv(converter, &inptr, &inleft, &outptr, &outleft);
    if (result != (size_t)-1) {
        *outptr = '\0';
        /* Success - conversion worked */
    }

    iconv_close(converter);
    /* Operations complete */

    return 0;
}

void test_compartment_boundary(void) {
    /* Can't use printf from compartment 2 when libc is protected */
    /* Function in compartment 2 */
}