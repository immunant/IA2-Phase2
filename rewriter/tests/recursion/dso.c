#include "recursion_main.h"
#include <criterion/criterion.h>
#include <ia2.h>
#include <stdio.h>

#define IA2_COMPARTMENT 2
#include <ia2_compartment_init.inc>

void recurse_dso(int count) {
  cr_log_info("recursion_dso: %d\n", count);
  if (count > 0) {
    recurse_main(count - 1);
  }
}
