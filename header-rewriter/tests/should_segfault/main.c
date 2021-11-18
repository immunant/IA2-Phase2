#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <print_secret.h>
#include <ia2.h>

uint32_t secret = 0xdeadbeef;

int main() {
    // This program should terminate in the sighandler so we should avoid
    // using printf since flushing stdout is not async-signal-safe
    printf("TRUSTED: the secret is %x\n", secret);
    print_secret();
}
