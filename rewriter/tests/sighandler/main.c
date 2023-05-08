#include "lib.h"
#include <stddef.h>
#include <signal.h>
#include <ia2.h>
#define IA2_DEFINE_TEST_HANDLER
#include <test_fault_handler.h>

INIT_RUNTIME(2);
INIT_COMPARTMENT(1);

void trap_handler(int sig) {
    const char *msg = "got here\n";
    write(1, msg, strlen(msg));
};

IA2_DEFINE_SIGHANDLER(trap_handler);

void install_sighandler(void) {
    static struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = ia2_sighandler_trap_handler;
    sigemptyset(&(sa.sa_mask));
    sigaction(SIGTRAP, &sa, NULL);
}

int main() {
    install_sighandler();
    raise(SIGTRAP);
    test_handler();
}
