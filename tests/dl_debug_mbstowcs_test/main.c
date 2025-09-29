#include "library.h"

#include <ia2.h>
#include <ia2_test_runner.h>
#include <locale.h>

INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

void ia2_main(void) {
  ia2_register_compartment("main", 1, NULL);
  ia2_register_compartment("libdl_debug_mbstowcs_test_lib.so", 2, NULL);
}

Test(dl_debug_mbstowcs_test, conversion_crosses_compartments) {
  const char *locale = setlocale(LC_ALL, "C.UTF-8");
  if (!locale) {
    locale = setlocale(LC_ALL, "en_US.UTF-8");
  }
  cr_assert(locale);
  cr_assert_eq(trigger_mbstowcs_dlopen(), 0);

  // TESTING FIX: Set PKRU=0 before test exits to allow destructors to run
  // This works around the compartment protection issues during cleanup
  __asm__ volatile(
    "xor %%eax, %%eax\n"
    "xor %%ecx, %%ecx\n"
    "xor %%edx, %%edx\n"
    "wrpkru\n"
    ::: "eax", "ecx", "edx"
  );
}
