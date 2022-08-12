#include "library.h"
#include <ia2.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

INIT_RUNTIME(1);
INIT_COMPARTMENT(1);

int data_in_main = 30;

void defined_in_main() { printf("main data is %d\n", data_in_main); }

IA2_DEFINE_WRAPPER(defined_in_main, _ZTSPFvvE, 1);

Fn fnptr_from_main = IA2_WRAPPER(defined_in_main, 1);

int main() {
  pid_t pid = fork();
  if (pid == 0) {
    printf("in child\n");
    library_foo();
  } else {
    if (pid < 0) {
      printf("fork() failed: %s\n", strerror(errno));
      exit(1);
    }
    int status;
    waitpid(pid, &status, 0);
    printf("in parent\n");
    library_foo();
  }
  defined_in_main();
  library_call_fn(fnptr_from_main);
  return 0;
}
