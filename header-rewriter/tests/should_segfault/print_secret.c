#include <stdio.h>
#include "print_secret.h"
#include "untrusted_segfault_handler.h"

void print_secret() {
    printf("UNTRUSTED: the secret is %x\n", secret);
}
