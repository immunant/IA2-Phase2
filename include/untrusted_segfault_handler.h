#include <signal.h>
#include <unistd.h>
#include <string.h>

// The test output should be checked to see that the segfault occurred at the
// expected place.
void handle_segfault(int sig) {
    if (sig == SIGSEGV) {
        // Write directly to stdout since printf is not async-signal-safe
        const char *msg = "UNTRUSTED: seg faulted as expected\n";
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
