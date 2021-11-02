#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include "print_secret.h"

void handle_segfault(int sig) {
    if (sig == SIGSEGV) {
        // Write directly to stdout since printf is not async-signal-safe
        const char *msg = "UNTRUSTED: seg faulted as expected\n";
        write(1, msg, strlen(msg));
        _exit(0);
    }
}

void print_secret() {
    signal(SIGSEGV, handle_segfault);
    printf("UNTRUSTED: the secret is %x\n", secret);
}
