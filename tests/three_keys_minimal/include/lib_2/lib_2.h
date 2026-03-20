#pragma once

#include "lib.h"

DECLARE_LIB(2, 1);

/**
 * Issue a non-MAP_FIXED anonymous mmap using an address hint from another
 * compartment.
 *
 * This is used by the regression test for tracer mmap policy. The kernel may
 * ignore a non-fixed hint and place the mapping elsewhere, so this operation
 * should be allowed by policy.
 *
 * @return 0 on success, or -errno on failure.
 */
int lib_2_mmap_nonfixed_hint(void *hint);
