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
    iconv_t converter = iconv_open("UTF-8", "ISO-8859-1");

    if (converter == (iconv_t)-1) {
        return -errno;
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
    int status = 0;

    inptr = *shared_inptr;
    outptr = *shared_outptr;
    inleft = *shared_inleft;
    outleft = *shared_outleft;

    shared_free(shared_inptr);
    shared_free(shared_outptr);
    shared_free(shared_inleft);
    shared_free(shared_outleft);

    if (result == (size_t)-1) {
        status = -errno;
    } else {
        *outptr = '\0';
    }

    shared_free(shared_input);
    shared_free(shared_output);

    iconv_close(converter);

    return status;
}

void test_compartment_boundary(void) {
}
