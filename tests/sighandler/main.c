/*
RUN: true
We need this because lib.c uses LINKARGS checks but not this file.
*/

#include "lib.h"
#include <stddef.h>
#include <signal.h>
#include <ia2.h>


#include <ia2_test_runner.h>

INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

void ia2_main(void) {
    ia2_register_compartment("main", 1, NULL);
    ia2_register_compartment("libsighandler_lib.so", 2, NULL);
}

struct handler {
    IA2_IGNORE(void (*handler)(int sig));
};

int main_secret = 3;

void trap_handler(int sig) {
    volatile int test = main_secret;
    volatile int test2 = CHECK_VIOLATION(lib_secret);
};

IA2_DEFINE_SIGHANDLER(trap_handler, 1);

void install_sighandler(struct handler *h) {
    static struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sigemptyset(&(sa.sa_mask));

    if (h) {
        sa.sa_handler = h->handler;
    } else {
        sa.sa_handler = IA2_SIGHANDLER(trap_handler);
    }
    sigaction(SIGTRAP, &sa, NULL);
}

Test(sighandler, main_rewriter) {
    install_sighandler(NULL);
    raise(SIGTRAP);
}

Test(sighandler, main_ignore) {
    static struct handler h = {
        .handler = IA2_SIGHANDLER(trap_handler),
    };
    install_sighandler(&h);
    raise(SIGTRAP);
}

Test(sighandler, lib_rewriter) {
    install_sighandler_in_lib(true);
    test_handler_from_lib();
}

Test(sighandler, lib_ignore) {
    install_sighandler_in_lib(false);
    test_handler_from_lib();
}
