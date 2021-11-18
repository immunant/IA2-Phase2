#include <stdio.h>
#include <stdint.h>
#include "foo.h"
#include "untrusted_indirect-original_fn_ptr_ia2.h"

static uint64_t secret = 0xcafed00d;

uint64_t pick_rhs(uint64_t x, uint64_t y) {
    return y;
}

uint64_t leak_secret_address(uint64_t x, uint64_t y) {
    return (uint64_t)&secret;
}

int main() {
    printf("TRUSTED: the secret is 0x%lx\n", secret);
    printf("0x%lx\n", apply_callback(1, 2));

    register_callback(IA2_FNPTR_WRAPPER(pick_rhs, _ZTSPFmmmE));
    printf("0x%lx\n", apply_callback(3, 4));

    register_callback(IA2_FNPTR_WRAPPER(leak_secret_address, _ZTSPFmmmE));
    printf("TRUSTED: oops we leaked the address of the secret\n");
    apply_callback(5, 6);

    unregister_callback();
}
