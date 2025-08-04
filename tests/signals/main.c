#include <ia2.h>
#include <ia2_test_runner.h>

INIT_RUNTIME(2);
#define IA2_COMPARTMENT 1
#include <ia2_compartment_init.inc>

#include <signal.h>

void ia2_main(void) {
  ia2_register_compartment("main", 1, NULL);
  ia2_register_compartment("libsignals_lib.so", 2, NULL);
}

Test(signals, signal_1, .signal = SIGHUP) {
  raise(1);
}

Test(signals, signal_2, .signal = SIGINT) {
  raise(2);
}

// `SIGQUIT` core dumps.
Test(signals, signal_3, .signal = SIGQUIT) {
  raise(3);
}

// `SIGILL` core dumps.
Test(signals, signal_4, .signal = SIGILL) {
  raise(4);
}

// `SIGTRAP` core dumps.
Test(signals, signal_5, .signal = SIGTRAP) {
  raise(5);
}

// `SIGABRT` core dumps.
Test(signals, signal_6, .signal = SIGABRT) {
  raise(6);
}

// TODO CHECK_VIOLATION: unexpected seg fault
// Test(signals, signal_7, .signal = SIGBUS) {
//     raise(7);
// }

// TODO CHECK_VIOLATION: unexpected seg fault
// Test(signals, signal_8, .signal = SIGFPE) {
//     raise(8);
// }

Test(signals, signal_9, .signal = SIGKILL) {
  raise(9);
}

Test(signals, signal_10, .signal = SIGUSR1) {
  raise(10);
}

// TODO CHECK_VIOLATION: unexpected seg fault
// Test(signals, signal_11, .signal = SIGSEGV) {
//     raise(11);
// }

Test(signals, signal_12, .signal = SIGUSR2) {
  raise(12);
}

Test(signals, signal_13, .signal = SIGPIPE) {
  raise(13);
}

Test(signals, signal_14, .signal = SIGALRM) {
  raise(14);
}

Test(signals, signal_15, .signal = SIGTERM) {
  raise(15);
}

Test(signals, signal_16, .signal = SIGSTKFLT) {
  raise(16);
}

// `SIGCHLD` ignored.
Test(signals, signal_17) {
  raise(SIGCHLD);
}

// `SIGCONT` ignored.
Test(signals, signal_18) {
  raise(SIGCONT);
}

// `SIGSTOP` stops.
Test(signals, signal_19) {
  // raise(SIGSTOP);
}

// `SIGTSTP` stops.
Test(signals, signal_20) {
  // raise(SIGTSTP);
}

// `SIGTTIN` stops.
Test(signals, signal_21) {
  // raise(SIGTTIN);
}

// `SIGTTOU` stops.
Test(signals, signal_22) {
  // raise(SIGTTOU);
}

// `SIGURG` ignored.
Test(signals, signal_23) {
  raise(SIGURG);
}

// `SIGXCPU` core dumps.
Test(signals, signal_24, .signal = SIGXCPU) {
  raise(24);
}

// `SIGXFSZ` core dumps.
Test(signals, signal_25, .signal = SIGXFSZ) {
  raise(25);
}

Test(signals, signal_26, .signal = SIGVTALRM) {
  raise(26);
}

Test(signals, signal_27, .signal = SIGPROF) {
  raise(27);
}

// `SIGWINCH` ignored.
Test(signals, signal_28) {
  raise(SIGWINCH);
}

Test(signals, signal_29, .signal = SIGIO) {
  raise(29);
}

Test(signals, signal_30, .signal = SIGPWR) {
  raise(30);
}

// `SIGSYS` core dumps.
Test(signals, signal_31, .signal = SIGSYS) {
  raise(31);
}

// Realtime signals ignored.
Test(signals, signal_32) {
  raise(32);
}
