#include <stdio.h>
#include "call_hook.h"
#include "trusted_indirect_ia2.h"

int main() {
    for (int i = 0; i < 2; i++) {
        F f = get_fn().op;
        int res = IA2_FNPTR_UNWRAPPER(f, _ZTSPFiiE)(32);
        //int res = ({
        //    int IA2_fnptr_wrapper_f(int __ia2_arg_0) {
        //        __libia2_untrusted_gate_push();
        //        int __res = ((int(*)(int))f.ptr)(__ia2_arg_0);
        //        __libia2_untrusted_gate_pop_ptr();
        //        return __res;
        //    }
        //    IA2_fnptr_wrapper_f;
        //})(32));
        printf("%d\n", res);
        change_fn();
    }
    return 0;
}
