#include <linux/elf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/uio.h>

#include "get_inferior_pkru.h"

/* this is largely copped from gdb and pared down to just what we need. it would
 * be much more complex if we had to deal with the compacted xsave area. */

#define X86_XSTATE_PKRU_SIZE 2696
#define X86_XSTATE_MAX_SIZE 2696

/* offset to the location of the PKRU register data structure used by the
 * "xsave" instruction */
static int xsave_pkeys_offset =
    2688 + 0 * 8; /* %pkru (64 bits in XSTATE, 32-bit actually used by
                     instructions and applications). */

bool get_inferior_pkru(pid_t pid, uint32_t *pkru_out) {
  char xstateregs[X86_XSTATE_MAX_SIZE];
  struct iovec iov;

  /* Pre-4.14 kernels have a bug (fixed by commit 0852b374173b
     "x86/fpu: Add FPU state copying quirk to handle XRSTOR failure on
     Intel Skylake CPUs") that sometimes causes the mxcsr location in
     xstateregs not to be copied by PTRACE_GETREGSET.  Make sure that
     the location is at least initialized with a defined value.  */
  memset(xstateregs, 0, sizeof(xstateregs));
  iov.iov_base = xstateregs;
  iov.iov_len = sizeof(xstateregs);
  if (ptrace(PTRACE_GETREGSET, pid, (unsigned int)NT_X86_XSTATE, (long)&iov) <
      0) {
    perror("could not read xstate registers");
    return false;
  }

  memcpy(pkru_out, &xstateregs[xsave_pkeys_offset], sizeof(*pkru_out));
  return true;
}
