/* XFAIL: *
   RUN: mkdir %t
   RUN: cp %s %t/main.c
   RUN: cp %S/foo.h %S/bar.h %S/baz.h %t/
   RUN: %clang %S/foo.c %S/bar.c %S/baz.c -shared -o %t/liboriginal.so
   RUN: ia2-header-rewriter %t/wrapper.c %t/foo.h %t/bar.h %t/baz.h
   RUN: cat %t/foo.h | FileCheck %S/foo.h
   RUN: cat %t/bar.h | FileCheck %S/bar.h
   RUN: cat %t/baz.h | FileCheck %S/baz.h
   RUN: %clang %t/wrapper.c -shared -Wl,--version-script,%t/wrapper.c.syms %t/liboriginal.so -o %t/libwrapper.so
   RUN: %clang %t/main.c -Wl,-rpath=%t %t/libwrapper.so -I%t -I%ia2_include -o %t/main
   RUN: %t/main | diff - %S/Output/main.out
 */

#include "foo.h"
#include "bar.h"
#include "baz.h"
#include <stdio.h>

int main() {
    printf("%d\n", foo2());
    printf("%d\n", bar());
    printf("%d\n", baz());
}
