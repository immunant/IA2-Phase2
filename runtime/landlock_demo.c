#include "forbid_paths.h"
#include "landlock.h"
#include "landlock_syscall.h"
#include "strv.h"

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(const int argc, char *const argv[], char *const *const envp) {
  const char *cmd_path;
  char *const *cmd_argv;

  if (argc < 2) {
    fprintf(stderr, "usage: DENY_PATH=... %s <command>\n\n", basename(argv[0]));
    fprintf(stderr, "built against landlock ABI version <= %d\n",
            LANDLOCK_ABI_LAST);
    return 1;
  }

  struct landlock_ctx ctx = {0};
  if (landlock_setup(&ctx) != 0) {
    return 1;
  }

  const char *path = getenv("DENY_PATH");
  const char *paths[] = {path, NULL};

  if (landlock_apply_path_blacklist(&ctx, paths) != 0) {
    return 1;
  }

  cmd_path = argv[1];
  cmd_argv = argv + 1;
  execvpe(cmd_path, cmd_argv, envp);
  perror("execvpe");
  return 1;
}
