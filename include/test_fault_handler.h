#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

// Configure the signal handler to expect an mpk violation when `expr` is
// evaluated. If `expr` doesn't trigger a fault, this macro manually raises a
// fault with a different message.
#define CHECK_VIOLATION(expr) ({       \
    fault = EXPECTED_FAULT;            \
    typeof(expr) _tmp = expr;          \
    fault = NO_FAULT;                  \
    raise(SIGSEGV);                    \
    _tmp;                              \
})                                     \

enum fault_cause {
    EARLY_FAULT,
    EXPECTED_FAULT,
    NO_FAULT,
};

static enum fault_cause fault = EARLY_FAULT;

// The test output should be checked to see that the segfault occurred at the
// expected place.
void handle_segfault(int sig) {
    if (sig == SIGSEGV) {
        // Write directly to stdout since printf is not async-signal-safe
        const char *ok_msg = "SIGNAL HANDLER: seg faulted as expected\n";
        const char *early_fault_msg = "SIGNAL HANDLER: unexpected seg fault\n";
        const char *no_fault_msg = "SIGNAL HANDLER: did not seg fault as expected\n";
        const char *unknown_fault_msg = "SIGNAL HANDLER: hit unreachable case when checking the fault cause\n";
        const char *msg;
        if (fault == EARLY_FAULT) {
            msg = early_fault_msg;
        } else if (fault == EXPECTED_FAULT) {
            msg = ok_msg;
        } else if (fault == NO_FAULT) {
            msg = no_fault_msg;
        } else {
            msg = unknown_fault_msg;
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
