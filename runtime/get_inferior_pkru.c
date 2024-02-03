#include <cpuid.h>
#include <linux/elf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/uio.h>

#include "get_inferior_pkru.h"

/* this is a minimal implementation that reads the PKRU from the extended state
stored in the xsave area. note that gdb incorrectly assumes pkru to be stored at
offset 2688 in xstate, which only seems to be true for processors with avx512 */

#define X86_XSTATE_MAX_SIZE 2696

/* the location of the xcr0 register in the xsave area. xcr0 is a mask of which
other state fields are present */
#define I386_LINUX_XSAVE_XCR0_OFFSET 464

/* extended state feature bit for the PKRU register */
#define X86_XSTATE_PKRU (1ULL << 9)

bool get_inferior_pkru(pid_t pid, uint32_t *pkru_out) {
  unsigned char xstateregs[X86_XSTATE_MAX_SIZE];
  struct iovec iov;

  /* Pre-4.14 kernels have a bug (fixed by commit 0852b374173b
     "x86/fpu: Add FPU state copying quirk to handle XRSTOR failure on
     Intel Skylake CPUs") that sometimes causes the mxcsr location in
     xstateregs not to be copied by PTRACE_GETREGSET.  Make sure that
     the location is at least initialized with a defined value.  */
  memset(xstateregs, 0x69, sizeof(xstateregs));
  iov.iov_base = xstateregs;
  iov.iov_len = sizeof(xstateregs);
  if (ptrace(PTRACE_GETREGSET, pid, (unsigned int)NT_X86_XSTATE, (long)&iov) <
      0) {
    perror("could not read xstate registers");
    return false;
  }

  /* read the xcr0 register to determine which fields have been saved */
  uint64_t xcr0 = 0;
  memcpy(&xcr0, &xstateregs[I386_LINUX_XSAVE_XCR0_OFFSET], sizeof(xcr0));

  /* abort if xcr0 claims PKRU is not present */
  if (!(xcr0 & X86_XSTATE_PKRU)) {
    fprintf(stderr, "XCR0 does not have PKRU bit set; could not read PKRU\n");
    return false;
  }

  /* offset to the location of the PKRU register in xstate */
  static unsigned int xstate_pkru_offset = 0;
  /* query CPUID for PKRU location in xstate; see §13.5.7 of the Intel 64 SDM */
  if (xstate_pkru_offset == 0) {
    unsigned int dummy = 0;
    int success = __get_cpuid_count(0x0d, 0x09, &dummy, &xstate_pkru_offset, &dummy, &dummy);
    if (!success) {
      fprintf(stderr, "failed to query CPUID(eax=0x0d, ecx=0x09):ebx for pkru offset in xstate\n");
      return false;
    }
  }

  memcpy(pkru_out, &xstateregs[xstate_pkru_offset], sizeof(*pkru_out));
  return true;
}
