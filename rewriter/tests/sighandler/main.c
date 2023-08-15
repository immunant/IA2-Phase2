/*
RUN: %binary_dir/tests/sighandler/sighandler_main_wrapped | diff %S/Output/main.out -
*/
#include "lib.h"
#include <stddef.h>
#include <signal.h>
#include <ia2.h>
#define IA2_DEFINE_TEST_HANDLER
#include <test_fault_handler.h>

INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>


struct handler {
    IA2_IGNORE(void (*handler)(int sig));
};

int main_secret = 3;

void trap_handler(int sig) {
    const char *main_msg = "trap_handler: main_secret is ";
    const char *lib_msg = "trap_handler: lib_secret is ";
    write(1, main_msg, strlen(main_msg));
    char num = '0' + main_secret;
    write(1, &num, 1);
    write(1, "\n", 1);
    write(1, lib_msg, strlen(lib_msg));
    num = '0' + CHECK_VIOLATION(lib_secret);
    write(1, &num, 1);
    write(1, "\n", 1);
};

IA2_DEFINE_SIGHANDLER(trap_handler, 1);

void install_sighandler(struct handler *h) {
    static struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sigemptyset(&(sa.sa_mask));

    if (h) {
        sa.sa_handler = h->handler;
    } else {
        sa.sa_handler = ia2_sighandler_trap_handler;
    }
    sigaction(SIGTRAP, &sa, NULL);
}

void test_handler(void) {
    raise(SIGTRAP);
    test_handler_from_lib();
}

int main() {
    setbuf(stdout, NULL);
    install_sighandler(NULL);
    test_handler();

    static struct handler h = {
        .handler = ia2_sighandler_trap_handler,
    };
    install_sighandler(&h);
    test_handler();
}
