#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

// Configure the signal handler to expect an mpk violation when `expr` is
// evaluated. If `expr` doesn't trigger a fault, this macro manually raises a
// fault with a different message.
#define CHECK_VIOLATION(expr) ({                                \
    expect_fault = true;                                        \
    typeof(expr) _tmp = expr;                                   \
    printf("CHECK_VIOLATION: did not seg fault as expected\n"); \
    exit(0);                                                    \
    _tmp;                                                       \
})                                                              \

static bool expect_fault = false;

// The test output should be checked to see that the segfault occurred at the
// expected place.
void handle_segfault(int sig) {
    if (sig == SIGSEGV) {
        // Write directly to stdout since printf is not async-signal-safe
        const char *ok_msg = "CHECK_VIOLATION: seg faulted as expected\n";
        const char *early_fault_msg = "CHECK_VIOLATION: unexpected seg fault\n";
        const char *msg;
        if (expect_fault) {
            msg = ok_msg;
        } else {
            msg = early_fault_msg;
        }
        write(1, msg, strlen(msg));
        _exit(0);
    }
}

// Installs the previously defined signal handler and disables buffering on
// stdout to allow using printf prior to the sighandler
__attribute__((constructor)) void install_segfault_handler(void) {
    setbuf(stdout, NULL);
    signal(SIGSEGV, handle_segfault);
}
