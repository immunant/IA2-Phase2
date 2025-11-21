#pragma once

#if defined(IA2_LIBC_COMPARTMENT) && IA2_LIBC_COMPARTMENT

#ifdef __cplusplus
extern "C" {
#endif

// This header is intentionally minimal. Exit callgate implementations are
// inlined directly into __wrap___cxa_finalize and do not expose public APIs.

#ifdef __cplusplus
}
#endif

#endif // IA2_LIBC_COMPARTMENT
