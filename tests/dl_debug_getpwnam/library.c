#include "library.h"

#include <pwd.h>
#include <ia2.h>

#define IA2_COMPARTMENT 2
#include <ia2_compartment_init.inc>

bool ia2_got_pw IA2_SHARED_DATA = false;

int trigger_getpwnam_load(void) {
  struct passwd *pw = getpwnam("root");
  ia2_got_pw = (pw != NULL);
  return ia2_got_pw ? 0 : -1;
}
