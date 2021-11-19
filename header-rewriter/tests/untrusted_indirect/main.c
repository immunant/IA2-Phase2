#include <stdio.h>
#include <stdint.h>
#include "foo.h"
#include "untrusted_indirect-original_fn_ptr_ia2.h"

static uint64_t secret = 0xcafed00d;

// This test defines some variables, constants and functions in unique
// sections as a way to stress test the linker script `padding.ld` which was
// required to fix this test's behavior.
uint32_t initialized_var __attribute__((section("my_var_section"))) = 0x11223344;
const uint32_t immutable_var __attribute__((section("my_const_var_section"))) = 0x55667788;
uint32_t uninit_var __attribute__((section("my_uninit_var_section")));

__asm__(".section my_alloc_section, \"a\"\n\
    .byte 0\n\
    .previous");

__asm__(".section my_write_section, \"w\"\n\
    .byte 0\n\
    .previous");

__asm__(".section my_executable_section, \"x\"\n\
    .byte 0\n\
    .previous");

__attribute__((section("my_fn_section"))) uint64_t pick_rhs(uint64_t x, uint64_t y) {
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
