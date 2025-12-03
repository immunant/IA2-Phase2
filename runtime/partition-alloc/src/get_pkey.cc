#include "get_pkey.h"

#if IA2_ENABLE && defined(__aarch64__) && defined(IA2_LIBC_COMPARTMENT) && IA2_LIBC_COMPARTMENT
#include <ia2_loader.h>
#endif

// These functions are duplicated here from libia2 so that we don't have to link
// the allocator against libia2 itself. We want users with libia2 disabled to
// still be able to call `shared_malloc` etc.

#ifdef __x86_64__
__attribute__((__visibility__("hidden")))
uint32_t
ia2_get_pkru() {
  uint32_t pkru = 0;
  __asm__ volatile("rdpkru"
                   : "=a"(pkru)
                   : "a"(0), "d"(0), "c"(0));
  return pkru;
}

__attribute__((__visibility__("hidden")))
size_t
ia2_get_pkey() {
  uint32_t pkru;
  __asm__("rdpkru"
          : "=a"(pkru)
          : "a"(0), "d"(0), "c"(0));

  size_t pkey;
  switch (pkru) {
  case 0xFFFFFFFC:
    pkey = 0;
    break;
  case 0xFFFFFFF0:
    pkey = 1;
    break;
  case 0xFFFFFFCC:
    pkey = 2;
    break;
  case 0xFFFFFF3C:
    pkey = 3;
    break;
  case 0xFFFFFCFC:
    pkey = 4;
    break;
  case 0xFFFFF3FC:
    pkey = 5;
    break;
  case 0xFFFFCFFC:
    pkey = 6;
    break;
  case 0xFFFF3FFC:
    pkey = 7;
    break;
  case 0xFFFCFFFC:
    pkey = 8;
    break;
  case 0xFFF3FFFC:
    pkey = 9;
    break;
  case 0xFFCFFFFC:
    pkey = 10;
    break;
  case 0xFF3FFFFC:
    pkey = 11;
    break;
  case 0xFCFFFFFC:
    pkey = 12;
    break;
  case 0xF3FFFFFC:
    pkey = 13;
    break;
  case 0xCFFFFFFC:
    pkey = 14;
    break;
  case 0x3FFFFFFC:
    pkey = 15;
    break;
  default:
    pkey = 0;
    break;
  }

  return pkey;
}
#endif

#ifdef __aarch64__
__attribute__((__visibility__("hidden")))
size_t
ia2_get_pkey() {
#if IA2_ENABLE && defined(IA2_LIBC_COMPARTMENT) && IA2_LIBC_COMPARTMENT
  if (ia2_in_loader_gate) {
    return 1;
  }
#endif

  size_t x18;
  asm("mov %0, x18"
      : "=r"(x18));
  return x18 >> 56;
}
#endif
