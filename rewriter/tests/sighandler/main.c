#include "lib.h"
#include <stddef.h>
#include <signal.h>
#include <ia2.h>
#define IA2_DEFINE_TEST_HANDLER
#include <test_fault_handler.h>

INIT_RUNTIME(2);
INIT_COMPARTMENT(1);


struct handler {
    IA2_IGNORE_FIELD(void (*handler)(int sig));
};

void trap_handler(int sig) {
    const char *msg = "called trap_handler\n";
    write(1, msg, strlen(msg));
};

IA2_DEFINE_SIGHANDLER(trap_handler);

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
    install_sighandler(NULL);
    test_handler();

    static struct handler h = {
        .handler = ia2_sighandler_trap_handler,
    };
    install_sighandler(&h);
    test_handler();
}
