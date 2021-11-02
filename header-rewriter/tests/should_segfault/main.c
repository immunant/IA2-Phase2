#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <print_secret.h>
#include <ia2.h>

uint32_t secret = 0xdeadbeef;

int main() {
    initialize_heap_pkey(NULL, 0);
    // This program should terminate in the sighandler so we should avoid
    // using printf since flushing stdout is not async-signal-safe
    char buf[40];
    sprintf(buf, "TRUSTED: the secret is %x\n", secret);
    write(1, buf, strlen(buf));
    print_secret();
}
