#include <stdio.h>
#include <stdint.h>
#include "call_hook.h"
#include "trusted_indirect_ia2.h"

static uint16_t secret = 0xabcd;

uint16_t user_default(uint16_t *addr) {
    return *addr;
}

int main() {
    //F untrusted_fn = get_fn().fn;
    //uint16_t untrusted_res = IA2_FNPTR_UNWRAPPER(untrusted_fn, _ZTSPFtPtE)(&secret);
    //printf("%x\n", untrusted_res);

    set_default(IA2_FNPTR_WRAPPER(user_default, _ZTSPFtPtE));

    F trusted_fn = get_fn().fn;
    uint16_t trusted_res = IA2_FNPTR_UNWRAPPER(trusted_fn, _ZTSPFtPtE)(&secret);
    printf("%x\n", trusted_res);
}
