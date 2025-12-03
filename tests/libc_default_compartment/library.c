#include "library.h"

#include <ia2.h>
#include <ia2_test_runner.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int call_libc_from_pkey0(void) {
  // The library deliberately stays in the shared compartment, so the PKRU
  // should still be 0 when we enter from main.
  cr_assert_eq(ia2_get_compartment(), 0);

  const char *message = "pkey0-libc";

  // Use tmpfile() to get a FILE* (not a builtin)
  FILE *tmp = tmpfile();
  cr_assert(tmp);

  // Use fileno() to get file descriptor (not a builtin)
  int fd = fileno(tmp);
  cr_assert(fd >= 0);

  // Write using fputs (not a builtin)
  int result = fputs(message, tmp);
  cr_assert(result >= 0);

  // Use fflush (not a builtin)
  result = fflush(tmp);
  cr_assert(result == 0);

  // Use feof and ferror (not builtins)
  cr_assert(!feof(tmp));
  cr_assert(!ferror(tmp));

  // Clear error indicator (not a builtin)
  clearerr(tmp);

  // Use fclose (not a builtin)
  result = fclose(tmp);
  cr_assert(result == 0);

  // Use getpid (not a builtin)
  pid_t pid = getpid();
  cr_assert(pid > 0);

  return 0;
}
