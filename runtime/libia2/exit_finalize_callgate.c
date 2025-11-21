#if defined(IA2_LIBC_COMPARTMENT) && IA2_LIBC_COMPARTMENT
#include "ia2_internal.h"

#include <stddef.h>

extern void __real___cxa_finalize(void *dso_handle);

#endif // IA2_LIBC_COMPARTMENT
