#include "library.h"
#include <stdio.h>

int data_in_lib = 900;

void library_foo() { printf("data in library: %d\n", data_in_lib); }

void library_call_fn(Fn what) {
  printf("in lib, about to call fnptr; lib data: %d\n", data_in_lib);
  what();
}
