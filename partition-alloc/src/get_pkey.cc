#include "get_pkey.h"

// These functions are duplicated here from libia2 so that we don't have to link
// the allocator against libia2 itself. We want users with libia2 disabled to
// still be able to call `shared_malloc` etc.

__attribute__((__visibility__("hidden")))
uint32_t ia2_get_pkru() {
  uint32_t pkru = 0;
  __asm__ volatile("rdpkru" : "=a"(pkru) : "a"(0), "d"(0), "c"(0));
  return pkru;
}

__attribute__((__visibility__("hidden")))
size_t ia2_get_pkey() {
  uint32_t pkru;
  __asm__("rdpkru" : "=a"(pkru) : "a"(0), "d"(0), "c"(0));
  switch (pkru) {
  case 0xFFFFFFFC: {
    return 0;
  }
  case 0xFFFFFFF0: {
    return 1;
  }
  case 0xFFFFFFCC: {
    return 2;
  }
  case 0xFFFFFF3C: {
    return 3;
  }
  case 0xFFFFFCFC: {
    return 4;
  }
  case 0xFFFFF3FC: {
    return 5;
  }
  case 0xFFFFCFFC: {
    return 6;
  }
  case 0xFFFF3FFC: {
    return 7;
  }
  case 0xFFFCFFFC: {
    return 8;
  }
  case 0xFFF3FFFC: {
    return 9;
  }
  case 0xFFCFFFFC: {
    return 10;
  }
  case 0xFF3FFFFC: {
    return 11;
  }
  case 0xFCFFFFFC: {
    return 12;
  }
  case 0xF3FFFFFC: {
    return 13;
  }
  case 0xCFFFFFFC: {
    return 14;
  }
  case 0x3FFFFFFC: {
    return 15;
  }
  // TODO: We currently treat any unexpected PKRU value as pkey 0 (the shared
  // heap) for simplicity since glibc(?) initializes the PKRU to 0x55555554
  // (usually). We don't set the PKRU until the first compartment transition, so
  // let's default to using the shared heap before our first wrpkru. When we
  // initialize the PKRU properly (see issue #95) we should probably abort when
  // we see unexpected PKRU values.
  default: {
    return 0;
  }
  }
}
