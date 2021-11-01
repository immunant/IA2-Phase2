#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "print_secret.h"

void handle_segfault(int sig) {
    printf("seg faulted as expected\n");
    _exit(0);
}

void print_secret() {
    signal(SIGSEGV, handle_segfault);
    printf("UNTRUSTED: the secret is %d\n", secret);
}
