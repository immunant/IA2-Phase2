/*
RUN: cat sighandler_call_gates_1.ld | FileCheck --check-prefix=LINKARGS %s
*/

#include "lib.h"
#include <signal.h>
#include <ia2.h>
#include <ia2_test_runner.h>

#define IA2_DEFINE_TEST_HANDLER


#define IA2_COMPARTMENT 2
#include <ia2_compartment_init.inc>

extern int main_secret;
int lib_secret = 4;

struct handler {
    IA2_IGNORE(void (*handler)(int sig));
};

void trap_handler_lib(int sig) {
    volatile int test = lib_secret;
    volatile int test2 = CHECK_VIOLATION(main_secret);
};

IA2_DEFINE_SIGHANDLER(trap_handler_lib, 2);

// LINKARGS: --wrap=test_handler_from_lib
void test_handler_from_lib(void) {
    raise(SIGTRAP);
}

void install_sighandler_lib(struct handler *h) {
    static struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sigemptyset(&(sa.sa_mask));

    if (h) {
        sa.sa_handler = h->handler;
    } else {
        sa.sa_handler = IA2_SIGHANDLER(trap_handler_lib);
    }
    sigaction(SIGTRAP, &sa, NULL);
    cr_log_info("Installed SIGTRAP handler in lib");
}

void install_sighandler_in_lib(bool rewrite) {
    if (rewrite) {
        install_sighandler_lib(NULL);
    } else {
        static struct handler h = {
            .handler = IA2_SIGHANDLER(trap_handler_lib),
        };
        install_sighandler_lib(&h);
    }
}
