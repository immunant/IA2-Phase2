#include "library.h"

#include <ia2.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define IA2_COMPARTMENT 2
#include <ia2_compartment_init.inc>

// Compartment-private counter (validates PKRU allows pkey 2 access)
static int compartment_counter = 0;

// Shared buffer (pkey 0) so kernel can read it during write() syscall
IA2_SHARED_DATA static char compartment_buffer[64];

// Shared sentinel flag to verify destructor ran (pkey 0, visible to main)
IA2_SHARED_DATA volatile int destructor_ran_flag = 0;

__attribute__((destructor)) static void exit_callgate_handler(void) {
  destructor_ran_flag = 1;  // Signal that destructor executed
  compartment_counter++;

  static const char prefix[] = "exit_callgate_check ";
  size_t len = sizeof(prefix) - 1;
  memcpy(compartment_buffer, prefix, len);

  int n = compartment_counter;
  size_t start = len;
  do {
    compartment_buffer[len++] = '0' + (n % 10);
    n /= 10;
  } while (n > 0);
  compartment_buffer[len++] = '\n';

  if (write(STDERR_FILENO, compartment_buffer, len) < 0) {
    _exit(1);
  }
}

void trigger_exit_callgate(void) {
  compartment_counter = 0;
  destructor_ran_flag = 0;  // Reset sentinel
  memset(compartment_buffer, 0, sizeof(compartment_buffer));
}

int get_destructor_ran_flag(void) {
  return destructor_ran_flag;
}
