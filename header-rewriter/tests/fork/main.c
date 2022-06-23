#include "library.h"
#include <ia2.h>

INIT_RUNTIME(1);
INIT_COMPARTMENT(1);

int data_in_main = 30;

void defined_in_main() { printf("data is %d\n", data_in_main); }

int main() {
  pid_t pid = fork();
  if (pid == 0) {
    printf("in child\n");
    foo();
  } else {
    if (pid < 0) {
      printf("fork() failed: %s\n", strerror(errno));
      exit(1);
    }
    wait(pid);
    printf("in parent\n");
    foo();
  }
  defined_in_main();
  return 0;
}
