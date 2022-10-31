#define IA2_INIT_COMPARTMENT 1
#include <ia2.h>

#include "library.h"

INIT_RUNTIME(1);

int data_in_main = 30;

void defined_in_main() { printf("data is %d\n", data_in_main); }

int main() {
  foo();
  defined_in_main();
}
