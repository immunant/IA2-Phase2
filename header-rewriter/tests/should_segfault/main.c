#include <stdio.h>
#include <stdint.h>
#include <print_secret.h>
#include <ia2.h>

uint32_t secret = 0xdeadbeef;

int main() {
    initialize_heap_pkey(NULL, 0);
    printf("the secret is %d\n", secret);
    print_secret();
}
