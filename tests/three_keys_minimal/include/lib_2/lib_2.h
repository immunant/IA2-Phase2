#pragma once

#include "lib.h"

DECLARE_LIB(2, 1);

/**
 * Issue a non-MAP_FIXED anonymous mmap using an address hint from another
 * compartment.
 *
 * This is used by the regression test for tracer mmap policy. With MAP_FIXED
 * unset, mmap must not replace any existing mapping, so this operation should
 * be allowed by policy even if the hint lands in another compartment.
 *
 * @return 0 on success, or -errno on failure.
 */
int lib_2_mmap_nonfixed_hint(void *hint);
