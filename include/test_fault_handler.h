#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#define LOG(...)              \
    printf("%s: ", __func__); \
    printf(__VA_ARGS__);      \
    printf("\n")

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

// This is shared data to allow checking for violations in multiple
// compartments. The attribute was added manually here to avoid including ia2.h
// since that would pull in libia2 as a dependency (the libia2 build generates a
// header included in ia2.h).
bool expect_fault __attribute__((section("ia2_shared_data"))) = false;

// The test output should be checked to see that the segfault occurred at the
// expected place.
void handle_segfault(int sig) {
#ifndef LIBIA2_INSECURE
    // Remove all MPK restrictions to ensure we can print a message regardless
    // of which compartment the violation occurred in.
    __asm__("wrpkru":: "a" (0), "c" (0), "d" (0));
#endif
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

bool handler_installed = false;

// Installs the previously defined signal handler and disables buffering on
// stdout to allow using printf prior to the sighandler
__attribute__((constructor)) void install_segfault_handler(void) {
    if (handler_installed) {
        return;
    }
    handler_installed = true;
    setbuf(stdout, NULL);
    signal(SIGSEGV, handle_segfault);
}
