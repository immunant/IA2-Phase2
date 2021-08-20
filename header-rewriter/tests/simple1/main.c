#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "simple1.h"

// Secret values: a secret string and decryption value.
// The untrusted compartment should not be able to read these.
static const char secret_string[] = "This is a secret.\n";
static int last_xor;

static int main_read(int i) {
  if (i >= sizeof(secret_string)) {
    return 0;
  }

  int x = secret_string[i];
  last_xor = rand();
  return x ? (x ^ last_xor) : x;
}

static void main_write(int x) {
  putchar(x);
}

static int main_map(int x) {
  return x ? (x ^ last_xor) : x;
}

int main() {
  struct SimpleCallbacks scb = {
    .read_cb = main_read,
    .write_cb = main_write,
  };

  struct Simple *s = simple_new(scb);
  if (s == NULL) {
    printf("Error allocating Simple\n");
    return -1;
  }

  srand(time(NULL));
  simple_foreach(s, main_map);
  simple_destroy(s);

  return 0;
}
