/*
 * Library interface for dl_debug test
 */

#ifndef DL_DEBUG_TEST_LIBRARY_H
#define DL_DEBUG_TEST_LIBRARY_H

// Function in compartment 2 that triggers iconv (and thus _dl_debug_state)
int trigger_iconv_dlopen(void);

// Simple test function for compartment boundary
void test_compartment_boundary(void);

#endif // DL_DEBUG_TEST_LIBRARY_H